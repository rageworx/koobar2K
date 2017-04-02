#include "audiodxsnd.h"

#include <D2D1.h>
#include <wincodec.h>
#include <mmsystem.h>
#include <dsound.h>

#include <cstdio>
#include <cstdlib>

#include "saferelease.h"

////////////////////////////////////////////////////////////////////////////////

using namespace D2D1;
using namespace std;

////////////////////////////////////////////////////////////////////////////////

#define DEF_ADXS_RESERVE_BUFF_SZ        50 * 1024 * 1024
#define DEF_ADXS_MIN_PLAYBUFF_SZ        256 * 1024
#define DEF_ADXS_MIN_NEXT_SZ            256

#define TIME_PER_SAMPLE( _s_ )          ( 1.0f / (float)_s_ )

////////////////////////////////////////////////////////////////////////////////

static IDirectSound*        pDSnd = NULL;
static IDirectSoundNotify*  pDsNotify = NULL;
static DSBPOSITIONNOTIFY    PositionNotify;
static HANDLE               hEventBuffer;

////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    unsigned                position;
    IDirectSoundBuffer*     pdxbuffer;
    bool                    played;
    bool                    endofbuffer;
}DXBufferState;

////////////////////////////////////////////////////////////////////////////////
// Buffer tool ---
IDirectSoundBuffer* createDSBufferRaw( const char* ref, int chnls, int rate, int size, DSBUFFERDESC &dsdesc );
bool FillDSound( IDirectSoundBuffer* &dsbuff, const char* ref, int chnls, int rate, int size, DSBUFFERDESC &dsdesc );
void* pthread_dxa_call( void* p );

////////////////////////////////////////////////////////////////////////////////

AudioDXSound::AudioDXSound( HWND hParent )
 : _hParent( hParent ),
   _inited( false ),
   _played( false ),
   _dxbidx_w( -1 ),
   _dxbidx_r( -1 ),
   _newctrlstate( NONE ),
   _ctrlstate( NONE ),
   _currentbufferque( 0 ),
   _threadkeeplived( true )
{
    _buffer.reserve( DEF_ADXS_RESERVE_BUFF_SZ );

    _dxbuff[0] = NULL;
    _dxbuff[1] = NULL;

    int reti = pthread_create( &_ptt, NULL, pthread_dxa_call, this );
}

AudioDXSound::~AudioDXSound()
{
    if ( _inited == true )
    {
        if ( _ctrlstate != NONE )
        {
            Control( STOP, 0, 0 );
        }

        _threadkeeplived = false;

        pthread_kill( _ptt, NULL );
        pthread_join( _ptt, NULL );

        FinalAudio();
    }
}

AudioOut::AResult AudioDXSound::InitAudio( unsigned chl, unsigned freq )
{
    if ( _inited == true )
        return ALEADYINITED;

    if ( pDSnd != NULL )
        return FAIL;

    HRESULT hr = E_FAIL;

    hr = DirectSoundCreate( NULL, &pDSnd, NULL );

    if ( hr == S_OK )
    {
        pDSnd->SetCooperativeLevel( _hParent, DSSCL_NORMAL );

        _channels  = chl;
        _frequency = freq;

        hEventBuffer = CreateEvent( NULL, TRUE, FALSE, NULL );

        return OK;
    }
#ifdef DEBUG
    else
    {
        printf("Failure: DirectSoundCreate()\n");
    }
#endif // DEBUG
    return FAIL;
}

AudioOut::AResult AudioDXSound::FinalAudio()
{
    if ( _inited == true )
    {
        SafeRelease( &pDSnd );
        _inited = false;

        return OK;
    }

    DeleteObject( hEventBuffer );

    return FAIL;
}

AudioOut::AResult AudioDXSound::ConfigAudio()
{
    return FAIL;
}

AudioOut::AResult AudioDXSound::WriteBuffer( const unsigned char* buffer, unsigned sz )
{
    if ( ( buffer != NULL ) && ( sz > 0 ) )
    {
        for( unsigned cnt=0; cnt<sz; cnt++ )
        {
            _buffer.push_back( buffer[ cnt ] );
        }

        if ( _currentbufferque < ( DEF_ADXS_MIN_PLAYBUFF_SZ * 2 ) )
        {
            if ( _buffer.size() > ( DEF_ADXS_MIN_PLAYBUFF_SZ * 2 ) )
            {
                // Make first double buffers..
                createNextBuffer();
                createNextBuffer();
            }
        }

        return OK;
    }
    return FAIL;
}

AudioOut::AResult AudioDXSound::ClearBuffer()
{
    if ( ( _ctrlstate == STOP ) || ( _ctrlstate == NONE ) )
    {
        _buffer.clear();

        return OK;
    }
    return FAIL;
}

AudioOut::AResult AudioDXSound::Control( ControlType ct, unsigned p1, unsigned p2 )
{
    // Check current control type.
    if ( _ctrlstate != ct )
    {
        _newctrlstate = ct;

        switch ( _newctrlstate )
        {
            case PLAY:
                {
                    if ( procPlay() == true )
                    {
                        _ctrlstate = _newctrlstate;
                    }
                }
                break;

            case STOP:
                {
                    procStop();

                    _ctrlstate = _newctrlstate;
                }
                break;
        }
    }

    return OK;
}

void* AudioDXSound::ThreadCall()
{
    unsigned            currentbuffer = 0;
    ControlType         currentctrlt  = NONE;

    while( _threadkeeplived == true )
    {
        if ( _ctrlstate == PLAY )
        {
            HRESULT hr = WaitForMultipleObjects( 1,
                                                 &hEventBuffer,
                                                 FALSE,
                                                 INFINITE );
            if ( hr == 0 )
            {
                ResetEvent( hEventBuffer );


                if ( _dxbidx_r < 0 )
                {
                    _dxbidx_r = 0;
                }

                int remidx = _dxbidx_r;
                bool islastend = false;

                DXBufferState* pdxb = (DXBufferState*)_dxbuff[ _dxbidx_r ];
                if ( pdxb != NULL )
                {
                    if ( pdxb->endofbuffer == false )
                    {
                        createNextBuffer();
                    }
                    else
                    {
                        islastend = true;
                    }
                }

                _dxbidx_r++;
                if ( _dxbidx_r > 1 )
                {
                    _dxbidx_r = 0;
                }

                pdxb = (DXBufferState*)_dxbuff[ _dxbidx_r ];
                if ( pdxb != NULL )
                {
                    if ( pdxb->played  == false )
                    {
                        pdxb->pdxbuffer->Play( 0, 0 , 0 );
                        pdxb->played = true;
                    }
                }

                pdxb = (DXBufferState*)_dxbuff[ remidx ];
                if ( pdxb != NULL )
                {
                    pdxb->pdxbuffer->Stop();
                    pdxb->pdxbuffer->Release();

                    delete pdxb;
                    _dxbuff[ remidx ] = NULL;
                }

                if ( islastend == true )
                {
                    Control( STOP, 0, 0 );
                }
            }
        }
        else
        {
            Sleep( 10 );
        }
    }
}

bool AudioDXSound::createNextBuffer()
{
    unsigned curbuffsz = _buffer.size();

    if ( curbuffsz > DEF_ADXS_MIN_PLAYBUFF_SZ )
    {
        const char* refbuff = (const char*)_buffer.data();

        if ( _dxbidx_w == -1 )
        {
            _dxbidx_w = 0;
        }

        // Check prebuffer already exists...
        if ( _dxbuff[ _dxbidx_w ] != NULL )
            return false;

        unsigned buffsz = DEF_ADXS_MIN_PLAYBUFF_SZ;
        bool islastbuff = false;

        if ( ( _currentbufferque + buffsz ) > curbuffsz )
        {
            buffsz = curbuffsz - _currentbufferque;
            islastbuff = true;
        }

        DSBUFFERDESC tmpDESC;
        IDirectSoundBuffer* newdxsb = NULL;
        newdxsb = createDSBufferRaw( &refbuff[ _currentbufferque ],
                                     _channels,
                                     _frequency,
                                     buffsz,
                                     tmpDESC );
        if ( newdxsb != NULL )
        {
            HRESULT hr = DS_OK;
            hr = newdxsb->QueryInterface( IID_IDirectSoundNotify,
                                          (void**)&pDsNotify );
            if ( hr == DS_OK )
            {
                PositionNotify.dwOffset = buffsz - 1;
                PositionNotify.hEventNotify = hEventBuffer;

                pDsNotify->SetNotificationPositions( 1, &PositionNotify );
            }
            else
            {
                newdxsb->Release();
                return false;
            }

            DXBufferState* dxbs = new DXBufferState;

            if ( dxbs != NULL )
            {
                dxbs->position  = _currentbufferque;
                dxbs->pdxbuffer = newdxsb;
                dxbs->played    = false;
                dxbs->endofbuffer = islastbuff;

                _dxbuff[ _dxbidx_w ] = (void*)dxbs;

                _currentbufferque += buffsz;
                _dxbidx_w++;

                if ( _dxbidx_w > 1 )
                {
                    _dxbidx_w = 0;
                }
            }
            else
            {
                newdxsb->Release();
                return false;
            }


            return true;
        }
    }

    return false;
}

bool AudioDXSound::procPlay()
{
    if ( _played == false )
    {
        if ( ( _dxbuff[0] != NULL ) && ( _dxbuff[1] != NULL ) )
        {
            DXBufferState* pdxb = (DXBufferState*)_dxbuff[ 0 ];

            if ( pdxb != NULL )
            {
                pdxb->pdxbuffer->Play( 0, 0, 0 );
                pdxb->played = true;

                _played = true;

                return true;
            }
        }
    }

    return false;
}

void AudioDXSound::procStop()
{
    for( unsigned cnt=0; cnt<2; cnt++ )
    {
        DXBufferState* remdxb = (DXBufferState*)_dxbuff[ cnt ];

        if ( remdxb != NULL )
        {
            remdxb->pdxbuffer->Release();
        }

        delete remdxb;
        _dxbuff[ cnt ] = NULL;
    }

    _dxbidx_w = -1;
    _dxbidx_r = -1;

    _buffer.clear();
}

////////////////////////////////////////////////////////////////////////////////

IDirectSoundBuffer* createDSBufferRaw( const char* ref, int chnls, int rate, int size, DSBUFFERDESC &dsdesc )
{
    if ( ref == NULL )
        return NULL;

    if ( ( chnls <= 0 ) || ( rate <= 0 ) || ( size <= 0 ) )
        return NULL;

    WAVEFORMATEX wavfmt = {0};

    wavfmt.cbSize = sizeof( WAVEFORMATEX );
    wavfmt.wFormatTag = WAVE_FORMAT_PCM;
    wavfmt.wBitsPerSample = 16;
    wavfmt.nChannels = chnls;
    wavfmt.nSamplesPerSec = rate;
    wavfmt.nBlockAlign = wavfmt.nChannels * wavfmt.wBitsPerSample / 8;
    wavfmt.nAvgBytesPerSec = wavfmt.nBlockAlign * wavfmt.nSamplesPerSec;

    ZeroMemory( &dsdesc, sizeof( DSBUFFERDESC ) );
    dsdesc.dwSize = sizeof( DSBUFFERDESC );
    dsdesc.dwFlags = DSBCAPS_STATIC | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS;
    dsdesc.dwBufferBytes = size;
    dsdesc.lpwfxFormat = &wavfmt;

    IDirectSoundBuffer* newDSBuffer = NULL;

    HRESULT hr = pDSnd->CreateSoundBuffer( &dsdesc, &newDSBuffer, NULL );

    if ( hr == S_OK )
    {
        void* pwData1 = NULL;
        void* pwData2 = NULL;
        DWORD lenData1 = 0;
        DWORD lenData2 = 0;

        newDSBuffer->Lock( 0,
                      dsdesc.dwBufferBytes,
                      &pwData1,
                      &lenData1,
                      &pwData2,
                      &lenData2,
                      0 );

        memcpy( pwData1, ref, dsdesc.dwBufferBytes );
        newDSBuffer->Unlock( pwData1, dsdesc.dwBufferBytes, NULL, 0 );

        return newDSBuffer;
    }

    return NULL;
}

bool FillDSound( IDirectSoundBuffer* &dsbuff, const char* ref, int chnls, int rate, int size, DSBUFFERDESC &dsdesc )
{
    if ( pDSnd == NULL )
        return false;

    WAVEFORMATEX wavfmt = {0};

    wavfmt.cbSize = sizeof( WAVEFORMATEX );
    wavfmt.wFormatTag = WAVE_FORMAT_PCM;
    wavfmt.wBitsPerSample = 16;
    wavfmt.nChannels = chnls;
    wavfmt.nSamplesPerSec = rate;
    wavfmt.nBlockAlign = wavfmt.nChannels * wavfmt.wBitsPerSample / 8;
    wavfmt.nAvgBytesPerSec = wavfmt.nBlockAlign * wavfmt.nSamplesPerSec;

    ZeroMemory( &dsdesc, sizeof( DSBUFFERDESC ) );
    dsdesc.dwSize = sizeof( DSBUFFERDESC );
    dsdesc.dwFlags = DSBCAPS_STATIC | DSBCAPS_CTRLPOSITIONNOTIFY;
    dsdesc.dwBufferBytes = size;
    dsdesc.lpwfxFormat = &wavfmt;

    HRESULT hr = pDSnd->CreateSoundBuffer( &dsdesc, &dsbuff, NULL );

    if ( hr == S_OK )
    {
        void* pwData1 = NULL;
        void* pwData2 = NULL;
        DWORD lenData1 = 0;
        DWORD lenData2 = 0;

        dsbuff->Lock( 0,
                      dsdesc.dwBufferBytes,
                      &pwData1,
                      &lenData1,
                      &pwData2,
                      &lenData2,
                      0 );

        memcpy( pwData1, ref, dsdesc.dwBufferBytes );
        dsbuff->Unlock( pwData1, dsdesc.dwBufferBytes, NULL, 0 );

        return true;
    }

    return false;
}

void* pthread_dxa_call( void* p )
{
    if ( p != NULL )
    {
        AudioDXSound* adxs = (AudioDXSound*)p;
        return adxs->ThreadCall();
    }

    return NULL;
}
