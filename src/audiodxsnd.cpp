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

#define DEF_ADXS_BUFFERS                2
#define DEF_ADXS_BUFFERLIMIT            (DEF_ADXS_BUFFERS-1)
#define DEF_ADXS_RESERVE_BUFF_SZ        50 * 1024 * 1024
#define DEF_ADXS_MIN_PLAYBUFF_SZ        256 * 1024
//#define DEF_ADXS_MIN_NEXT_SZ            384
#define DEF_ADXS_MIN_NEXT_SZ            64

#define TIME_PER_SAMPLE( _s_ )          ( 1.0f / (float)_s_ )

#define DEBUG_SND_POSITION

////////////////////////////////////////////////////////////////////////////////

static IDirectSound*        pDSnd = NULL;
static IDirectSoundNotify*  pDsNotify = NULL;
static HANDLE               hEventBuffer;

////////////////////////////////////////////////////////////////////////////////

typedef enum
{
    NOTBUFFERED         = 0,
    BUFFERED,
    PLAYING,
    PLAYED
}bufferstatus;

typedef struct
{
    unsigned                position;
    unsigned                size;
    IDirectSoundBuffer*     pdxbuffer;
    bufferstatus            status;
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
    for( unsigned cnt=0; cnt<DEF_ADXS_BUFFERS; cnt++ )
    {
        _dxbuff[ cnt ] = NULL;
    }
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
    {
        if ( _channels != chl )
        {
            _channels = chl;
        }

        if ( _frequency != freq )
        {
            _frequency = freq;
        }

        return ALEADYINITED;
    }

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
        _prevbufferque = 0;

        hEventBuffer = CreateEvent( NULL, TRUE, FALSE, NULL );

        int reti = pthread_create( &_ptt, NULL, pthread_dxa_call, this );

        return OK;
    }
#ifdef DEBUG_AUDIO
    else
    {
        printf("Failure: DirectSoundCreate()\n");
    }
#endif // DEBUG_AUDIO
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

        for( unsigned cnt=0; cnt<2; cnt++ )
        {
            DXBufferState* remdxb = (DXBufferState*)&_dxbuff[ cnt ];

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

        SafeRelease( &pDSnd );
        _inited = false;

        DeleteObject( hEventBuffer );

        return OK;
    }


    return FAIL;
}

AudioOut::AResult AudioDXSound::ConfigAudio()
{
    // No options for DXSound.
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
            // Create buffers for play.
            unsigned bsz = _buffer.size();
            unsigned dsz = DEF_ADXS_MIN_PLAYBUFF_SZ * DEF_ADXS_BUFFERS;
            if ( bsz > dsz )
            {
                for( unsigned cnt=0; cnt<DEF_ADXS_BUFFERS; cnt++)
                {
                    createNextBuffer();
                }
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
                        _prevbufferque = 0;
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

                bool islastend = false;
                bool neednextbuff = false;

                DXBufferState* playedbuff = (DXBufferState*)_dxbuff[ _dxbidx_r ];

                // Switch buffer index.
                _dxbidx_r++;
                if ( _dxbidx_r > DEF_ADXS_BUFFERLIMIT )
                {
                    _dxbidx_r = 0;
                }

                DXBufferState* pdxb = (DXBufferState*)_dxbuff[ _dxbidx_r ];
                if ( pdxb != NULL )
                {
                    if ( pdxb->status == BUFFERED )
                    {
                        if ( ( _prevbufferque < pdxb->position ) &&
                             ( pdxb->position < _buffer.size() ) )
                        {
                            pdxb->pdxbuffer->Play( 0, 0, 0 );
                            pdxb->status = PLAYING;
#ifdef DEBUG_AUDIO_BUFFER_INDEX
                            printf( "Continued play _dxbidx_r = %d ( pos = %d / %d )\n",
                                    _dxbidx_r,
                                    pdxb->position,
                                    _buffer.size() );
#endif // DEBUG_AUDIO_BUFFER_INDEX

                            _prevbufferque = pdxb->position;
                        }
                        else
                        {
                            pdxb->status = PLAYED;
                            islastend = true;
                        }
                    }
                }

                if ( playedbuff != NULL )
                {
                    // Make Volume to Zero for removing popping noise !
                    playedbuff->status = PLAYED;

                    if ( playedbuff->endofbuffer == false )
                    {
                        // Generate next buffer.
                        createNextBuffer();
                    }
                    else
                    {
                        islastend = true;
                    }
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

    if ( _dxbidx_w < 0 )
    {
        _dxbidx_w = 0;
    }

    unsigned buffsz = DEF_ADXS_MIN_PLAYBUFF_SZ + DEF_ADXS_MIN_NEXT_SZ;
    bool islastbuff = false;

    if ( _dxbuff[ _dxbidx_w ] == NULL )
    {
#ifdef DEBUG_AUDIO_BUFFER_INDEX
        printf( "new buff index of _dxbidx_w = %d\n", _dxbidx_w );
#endif // DEBUG_AUDIO_BUFFER_INDEX

        unsigned realbuffsz = buffsz - DEF_ADXS_MIN_NEXT_SZ;
        unsigned cbbuffpos  = realbuffsz - DEF_ADXS_MIN_NEXT_SZ;

        if ( ( _currentbufferque + realbuffsz ) > curbuffsz )
        {
            buffsz     = curbuffsz - _currentbufferque;
            realbuffsz = buffsz;
            cbbuffpos  = buffsz;
            islastbuff = true;
        }

        // I don't know how remove popping noise while changing buffer,
        // So I made a silent buffer after real audio data.
        // Expect it will good for removing popping noise.
        char* copybuff = new char[ buffsz ];

        if ( copybuff == NULL )
            return false;

        memset( copybuff, 0, buffsz );
        memcpy( copybuff, &refbuff[ _currentbufferque ], realbuffsz );

#ifndef USE_EXACT_BUFFER_SIZE
        // Make more buffer to appended.
        if ( cbbuffpos < realbuffsz )
        {
            if ( curbuffsz < ( _currentbufferque + realbuffsz ) )
            {
                unsigned moresz = DEF_ADXS_MIN_NEXT_SZ / 2;
                memcpy( &copybuff[ realbuffsz ],
                        &refbuff[ _currentbufferque + realbuffsz ],
                        moresz );
            }
        }
#endif // USE_SIDE_BUFFER

        DSBUFFERDESC tmpDESC;
        IDirectSoundBuffer* newdxsb = NULL;
        newdxsb = createDSBufferRaw( copybuff,
                                     _channels,
                                     _frequency,
                                     buffsz,
                                     tmpDESC );

        delete[] copybuff;

        if ( newdxsb != NULL )
        {
            HRESULT hr = DS_OK;

            DSBPOSITIONNOTIFY PositionNotify;

            hr = newdxsb->QueryInterface( IID_IDirectSoundNotify,
                                          (void**)&pDsNotify );
            if ( hr == DS_OK )
            {
                DWORD offsz = cbbuffpos;
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
                dxbs->status    = BUFFERED;
                dxbs->endofbuffer = islastbuff;

                _dxbuff[ _dxbidx_w ] = (void*)dxbs;

                if ( realbuffsz < buffsz )
                {
                    _currentbufferque += realbuffsz;
                }
                else
                {
                    _currentbufferque += buffsz;
                }

                _dxbidx_w++;
                if ( _dxbidx_w > DEF_ADXS_BUFFERLIMIT )
                {
                    _dxbidx_w = 0;
                }
            }
            else
            {
                newdxsb->Release();
                _dxbuff[ _dxbidx_w ] = NULL;
                return false;
            }

            if ( _event != NULL )
            {
                _event->OnNewBuffer( _currentbufferque - buffsz, buffsz, 0xFFFFFFFF );
            }

            return true;
        }
    }
    else
    {
#ifdef DEBUG_AUDIO_BUFFER_INDEX
        printf( "resued buff index of _dxbidx_w = %d\n", _dxbidx_w );
#endif // DEBUG_AUDIO_BUFFER_INDEX
        DXBufferState* dxbs = (DXBufferState* )_dxbuff[ _dxbidx_w ];

        if ( dxbs == NULL )
            return false;

        if ( ( dxbs->status == BUFFERED ) || ( dxbs->status == PLAYING ) )
            return false;

        if ( ( _currentbufferque + buffsz ) >= curbuffsz )
        {
            buffsz = curbuffsz - _currentbufferque;
            islastbuff = true;
        }

        DSBUFFERDESC tmpDESC;
        IDirectSoundBuffer* dxsb = dxbs->pdxbuffer;

        if ( buffsz == 0 )
        {
            dxbs->status = NOTBUFFERED;
            return false;
        }

        bool retb = FillDSound( dxsb, &refbuff[ _currentbufferque ],
                                _channels, _frequency, buffsz, tmpDESC );
        if ( retb == false )
        {
#ifdef DEBUG_AUDIO_BUFFER_INDEX
            printf( "Error: Fill buffer failed.\n" );
#endif // DEBUG_AUDIO_BUFFER_INDEX
            return false;
        }

        _currentbufferque += buffsz;

        _dxbidx_w++;
        if ( _dxbidx_w > DEF_ADXS_BUFFERLIMIT )
        {
            _dxbidx_w = 0;
        }

        dxbs->status   = BUFFERED;
        dxbs->position = _currentbufferque;
        dxbs->size     = buffsz;
        dxbs->endofbuffer = islastbuff;

        if ( _event != NULL )
        {
            _event->OnNewBuffer( _currentbufferque - buffsz, buffsz, curbuffsz );
        }

        return true;
    }

    return false;
}

bool AudioDXSound::procPlay()
{
    if ( _played == false )
    {
        unsigned bufferedcnt = 0;

        for( unsigned cnt=0; cnt<DEF_ADXS_BUFFERS; cnt++ )
        {
            DXBufferState* pdxb = (DXBufferState*)_dxbuff[ cnt ];

            if ( pdxb != NULL )
            {
                if ( pdxb->status == BUFFERED )
                {
                    bufferedcnt++;
                }
            }
        }

        if ( bufferedcnt > DEF_ADXS_BUFFERLIMIT )
        {
            DXBufferState* pdxb = (DXBufferState*)_dxbuff[ 0 ];

            pdxb->pdxbuffer->Play( 0, 0, 0 );
            pdxb->status = PLAYED;

            _played = true;

            return true;
        }
    }

    return false;
}

void AudioDXSound::procStop()
{
    for( unsigned cnt=0; cnt<2; cnt++ )
    {
        DXBufferState* pdxb = (DXBufferState*)_dxbuff[ cnt ];

        if ( pdxb != NULL )
        {
            if ( pdxb->status == PLAYING )
            {
                if ( pdxb->pdxbuffer != NULL )
                {
                    pdxb->pdxbuffer->Stop();
                }
            }

            pdxb->status   = NOTBUFFERED;
            pdxb->position = 0;
        }
    }

    _dxbidx_w = -1;
    _dxbidx_r = -1;

    _buffer.clear();
    _buffer.resize(0);

    _currentbufferque = 0;
    _ctrlstate = NONE;

    _played = false;

#ifdef DEBUG_AUDIO
    printf("void AudioDXSound::procStop() done.\n" );
#endif // DEBUG_AUDIO
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

#ifdef DEBUG_AUDIO
    printf("Error: Failed to create createDSBufferRaw();\n");
#endif // DEBUG_AUDIO
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
    dsdesc.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS;
    dsdesc.dwBufferBytes = size;
    dsdesc.lpwfxFormat = &wavfmt;

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
