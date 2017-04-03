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
#define DEF_ADXS_MIN_PLAYBUFF_SZ        512 * 1024
//#define DEF_ADXS_MIN_NEXT_SZ            384
#define DEF_ADXS_MIN_NEXT_SZ            ( DEF_ADXS_MIN_PLAYBUFF_SZ / 2048 )

#define TIME_PER_SAMPLE( _s_ )          ( 1.0f / (float)_s_ )

#define DEBUG_SND_POSITION

////////////////////////////////////////////////////////////////////////////////

static IDirectSound*        pDSnd = NULL;
static IDirectSoundNotify*  pDsNotify = NULL;
static HANDLE               hEventBuffer;

////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    unsigned                position;
    unsigned                size;
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

AudioDXSound::AudioDXSound( HWND hParent, AudioOutEvent* e  )
 : AudioOut::AudioOut( e ),
   _hParent( hParent ),
   _inited( false ),
   _played( false ),
   _dxbidx_w( -1 ),
   _dxbidx_r( -1 ),
   _newctrlstate( NONE ),
   _ctrlstate( NONE ),
   _currentbufferque( 0 ),
   _threadkeeplived( true )
{
    _dxbuff[0] = NULL;
    _dxbuff[1] = NULL;
}

AudioDXSound::~AudioDXSound()
{
    if ( _inited == true )
    {
        if ( _ctrlstate != NONE )
        {
            Control( STOP, 0, 0 );
        }

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

        _buffer.clear();
        _buffer.reserve( DEF_ADXS_RESERVE_BUFF_SZ );

        hEventBuffer = CreateEvent( NULL, TRUE, FALSE, NULL );

        int reti = pthread_create( &_ptt, NULL, pthread_dxa_call, this );

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
        _threadkeeplived = false;

        //pthread_kill( _ptt, NULL );
        pthread_join( _ptt, NULL );

        procStop();

        SafeRelease( &pDSnd );
        _inited = false;

        DeleteObject( hEventBuffer );

        return OK;
    }


    return FAIL;
}

AudioOut::AResult AudioDXSound::ConfigAudio()
{
    return FAIL;
}

unsigned AudioDXSound::BufferPosition()
{
    return _currentbufferque;
}

AudioOut::AResult AudioDXSound::WriteBuffer( const unsigned char* buffer, unsigned sz )
{
    if ( ( buffer != NULL ) && ( sz > 0 ) )
    {
        for( unsigned cnt=0; cnt<sz; cnt++ )
        {
            _buffer.push_back( buffer[ cnt ] );
        }

        if ( _currentbufferque == 0 )
        {
            // Wait size filled in double buffered.
            unsigned bsz = _buffer.size();
            unsigned dsz = DEF_ADXS_MIN_PLAYBUFF_SZ * 2;
            if ( bsz > dsz )
            {
                createNextBuffer();
                createNextBuffer();
#ifdef DEBUG
                printf( "buffer size = %d, cached.\n", bsz );
#endif // DEBUG
            }
        }

        return OK;
    }
    return FAIL;
}

AudioOut::AResult AudioDXSound::FlushBuffer()
{
    if ( _currentbufferque < _buffer.size() )
    {
        createNextBuffer( true );
    }
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
                    ClearBuffer();

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
                bool neednextbuff = false;

                DXBufferState* beremoved = (DXBufferState*)_dxbuff[ remidx ];
                if ( beremoved != NULL )
                {
                    // Make Volume to Zero for removing popping noise !
                    beremoved->pdxbuffer->SetVolume( 0 );

                    if ( beremoved->endofbuffer == false )
                    {
                        // Generate next buffer.
                        createNextBuffer();
                    }
                    else
                    {
                        islastend = true;
                    }
                }

                _dxbuff[ remidx ] = NULL;

                // Switch buffer index.
                _dxbidx_r++;
                if ( _dxbidx_r > 1 )
                {
                    _dxbidx_r = 0;
                }

                DXBufferState* pdxb = (DXBufferState*)_dxbuff[ _dxbidx_r ];
                if ( pdxb != NULL )
                {
                    if ( pdxb->played  == false )
                    {
                        pdxb->pdxbuffer->Play( 0, 0, 0 );
                        pdxb->played = true;
                    }
                }

                if ( beremoved != NULL )
                {
                    beremoved->pdxbuffer->Lock( 0, 0, NULL, 0, NULL, 0, 0 );
                    beremoved->pdxbuffer->Unlock( NULL, 0, NULL, 0 );
                    beremoved->pdxbuffer->Release();

                    delete beremoved;
                }

                if ( islastend == true )
                {
                    if ( _event != NULL )
                    {
                        _event->OnBufferEnd();
                    }
                }
            }
        }
        else
        {
            Sleep( 1 );
        }
    }
}

bool AudioDXSound::createNextBuffer( bool flushleft )
{
    unsigned curbuffsz = _buffer.size();

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

        DSBPOSITIONNOTIFY PositionNotify;

        hr = newdxsb->QueryInterface( IID_IDirectSoundNotify,
                                      (void**)&pDsNotify );
        if ( hr == DS_OK )
        {
            DWORD offsz = buffsz - DEF_ADXS_MIN_NEXT_SZ;
            if ( ( (long long)buffsz - DEF_ADXS_MIN_NEXT_SZ) < 0 )
            {
                offsz = buffsz;
            }

            PositionNotify.dwOffset = offsz;
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
            dxbs->size      = buffsz;
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

        if ( _event != NULL )
        {
            _event->OnNewBuffer( _currentbufferque - buffsz, buffsz, curbuffsz );
        }

        return true;
    }
#ifdef DEBUG
    else
    {
        printf(" createDSBufferRaw failed.\n" );
    }
#endif // DEBUG
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
            if ( remdxb->pdxbuffer != NULL )
            {
                remdxb->pdxbuffer->Stop();
                remdxb->pdxbuffer->Release();
            }
        }

        delete remdxb;

        _dxbuff[ cnt ] = NULL;
    }

    _dxbidx_w = -1;
    _dxbidx_r = -1;

    _buffer.clear();
    _buffer.resize(0);

    _currentbufferque = 0;
    _ctrlstate = NONE;

    _played = false;

#ifdef DEBUG
    printf("void AudioDXSound::procStop() done.\n" );
#endif // DEBUG
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
    //dsdesc.dwFlags = DSBCAPS_STATIC | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS;
    dsdesc.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS;
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

#ifdef DEBUG
    printf("Error: Failed to create createDSBufferRaw();\n");
#endif // DEBUG
    return NULL;
}

bool FillDSound( IDirectSoundBuffer* &dsbuff, const char* ref, int chnls, int rate, int size, DSBUFFERDESC &dsdesc )
{
    if ( pDSnd == NULL )
        return false;

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

void* pthread_dxa_call( void* p )
{
    if ( p != NULL )
    {
        AudioDXSound* adxs = (AudioDXSound*)p;
        return adxs->ThreadCall();
    }

    return NULL;
}
