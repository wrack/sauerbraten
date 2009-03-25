// sound.cpp: basic positional sound using sdl_mixer

#include "cube.h"
#include "engine.h"

#include "SDL_mixer.h"
#define MAXVOL MIX_MAX_VOLUME
Mix_Music *mod = NULL;

bool nosound = true;

struct soundsample
{
    char *name;
    Mix_Chunk *chunk;

    soundsample() : name(NULL) {}
    ~soundsample() { DELETEA(name); }
};

struct soundslot
{
    soundsample *sample;
    int volume;
    int uses, maxuses;
};

struct soundchannel
{ 
    int id;
    bool inuse;
    vec loc; 
    soundslot *slot; 
    extentity *ent; 
    int volume, pan;
    bool dirty;

    soundchannel(int id) : id(id) { reset(); }

    bool hasloc() const { return loc.x >= -1e15f; }

    void reset()
    {
        inuse = false;
        loc = vec(-1e16f, -1e16f, -1e16f);
        slot = NULL;
        ent = NULL;
        volume = -1;
        pan = -1;
        dirty = false;
    }
};
vector<soundchannel> channels;
int maxchannels = 0;

soundchannel &newchannel(int n, soundslot *slot, const vec *loc = NULL, extentity *ent = NULL)
{
    if(ent)
    {
        loc = &ent->o;
        ent->visible = true;
        if(slot) slot->uses++;
    }
    while(!channels.inrange(n)) channels.add(channels.length());
    soundchannel &chan = channels[n];
    chan.reset();
    chan.inuse = true;
    if(loc) chan.loc = *loc;
    chan.slot = slot;
    chan.ent = ent;
    return chan;
}

void freechannel(int n)
{
    // Note that this can potentially be called from the SDL_mixer audio thread.
    // Be careful of race conditions when checking chan.inuse without locking audio.
    // Can't use Mix_Playing() checks due to bug with looping sounds in SDL_mixer.
    if(!channels.inrange(n) || !channels[n].inuse) return;
    soundchannel &chan = channels[n];
    chan.inuse = false;
    if(chan.ent) 
    {
        chan.ent->visible = false;
        if(chan.slot) chan.slot->uses--;
    }
}

void syncchannel(soundchannel &chan)
{
    if(!chan.dirty) return;
    if(!Mix_FadingChannel(chan.id)) Mix_Volume(chan.id, chan.volume);
    Mix_SetPanning(chan.id, 255-chan.pan, chan.pan);
    chan.dirty = false;
}

void setmusicvol(int musicvol)
{
    if(nosound) return;
    if(mod) Mix_VolumeMusic((musicvol*MAXVOL)/255);
}

void stopchannels()
{
    loopv(channels)
    {
        soundchannel &chan = channels[i];
        if(!chan.inuse) continue;
        Mix_HaltChannel(i);
        freechannel(i);
    }
}

VARFP(soundvol, 0, 255, 255, if(!soundvol) { stopchannels(); setmusicvol(0); });
VARFP(musicvol, 0, 128, 255, setmusicvol(soundvol ? musicvol : 0));

char *musicfile = NULL, *musicdonecmd = NULL;

void stopmusic()
{
    if(nosound) return;
    DELETEA(musicfile);
    DELETEA(musicdonecmd);
    if(mod)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(mod);
        mod = NULL;
    }
}

VARF(soundchans, 1, 32, 128, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(soundfreq, 0, MIX_DEFAULT_FREQUENCY, 44100, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(soundbufferlen, 128, 1024, 4096, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));

void initsound()
{
    if(Mix_OpenAudio(soundfreq, MIX_DEFAULT_FORMAT, 2, soundbufferlen)<0)
    {
        conoutf(CON_ERROR, "sound init failed (SDL_mixer): %s", (size_t)Mix_GetError());
        return;
    }
	Mix_AllocateChannels(soundchans);	
    Mix_ChannelFinished(freechannel);
    maxchannels = soundchans;
    nosound = false;
}

void musicdone()
{
    if(!musicdonecmd) return;
    if(mod) Mix_FreeMusic(mod);
    mod = NULL;
    DELETEA(musicfile);
    char *cmd = musicdonecmd;
    musicdonecmd = NULL;
    execute(cmd);
    delete[] cmd;
}

void music(char *name, char *cmd)
{
    if(nosound) return;
    stopmusic();
    if(soundvol && musicvol && *name)
    {
        if(cmd[0]) musicdonecmd = newstring(cmd);
        s_sprintfd(sn)("packages/%s", name);
        const char *file = findfile(path(sn), "rb");
        if((mod = Mix_LoadMUS(file)))
        {
            musicfile = newstring(file);
            Mix_PlayMusic(mod, cmd[0] ? 0 : -1);
            Mix_VolumeMusic((musicvol*MAXVOL)/255);
        }
        else
        {
            conoutf(CON_ERROR, "could not play music: %s", sn);
        }
    }
}

COMMAND(music, "ss");

hashtable<const char *, soundsample> samples;
vector<soundslot> gamesounds, mapsounds;

int findsound(const char *name, int vol, vector<soundslot> &sounds)
{
    loopv(sounds)
    {
        if(!strcmp(sounds[i].sample->name, name) && (!vol || sounds[i].volume==vol)) return i;
    }
    return -1;
}

int addsound(const char *name, int vol, int maxuses, vector<soundslot> &sounds)
{
    soundsample *s = samples.access(name);
    if(!s)
    {
        char *n = newstring(name);
        s = &samples[n];
        s->name = n;
        s->chunk = NULL;
    }
    soundslot *oldsounds = sounds.getbuf();
    int oldlen = sounds.length();
    soundslot &slot = sounds.add();
    // sounds.add() may relocate slot pointers
    if(sounds.getbuf() != oldsounds) loopv(channels)
    {
        soundchannel &chan = channels[i];
        if(chan.inuse && chan.slot >= oldsounds && chan.slot < &oldsounds[oldlen])
            chan.slot = &sounds[chan.slot - oldsounds];
    }
    slot.sample = s;
    slot.volume = vol ? vol : 100;
    slot.uses = 0;
    slot.maxuses = maxuses;
    return oldlen;
}

void registersound(char *name, int *vol) { intret(addsound(name, *vol, 0, gamesounds)); }
COMMAND(registersound, "si");

void mapsound(char *name, int *vol, int *maxuses) { intret(addsound(name, *vol, *maxuses < 0 ? 0 : max(1, *maxuses), mapsounds)); }
COMMAND(mapsound, "sii");

void resetchannels()
{
    loopv(channels) if(channels[i].inuse) freechannel(i);
    channels.setsize(0);
}

void clear_sound()
{
    closemumble();
    if(nosound) return;
    stopmusic();
    Mix_CloseAudio();
    resetchannels();
    gamesounds.setsizenodelete(0);
    mapsounds.setsizenodelete(0);
    samples.clear();
}

void clearmapsounds()
{
    loopv(channels) if(channels[i].inuse && channels[i].ent)
    {
        Mix_HaltChannel(i);
        freechannel(i);
    }
    mapsounds.setsizenodelete(0);
}

void checkmapsounds()
{
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type!=ET_SOUND || e.visible || camera1->o.dist(e.o)>=e.attr2) continue;
        playsound(e.attr1, NULL, &e);
    }
}

VAR(stereo, 0, 1, 1);

VARP(maxsoundradius, 0, 340, 10000);

bool updatechannel(soundchannel &chan)
{
    if(!chan.slot) return false;
    int vol = soundvol, pan = 255/2;
    if(chan.hasloc())
    {
        vec v;
        float dist = camera1->o.dist(chan.loc, v);
        if(chan.ent)
        {
            int rad = chan.ent->attr2;
            if(chan.ent->attr3)
            {
                rad -= chan.ent->attr3;
                dist -= chan.ent->attr3;
            }
            vol -= (int)(clamp(dist/rad, 0.0f, 1.0f)*soundvol);
        }
        else if(maxsoundradius)
        {
            vol -= (int)(clamp(dist/maxsoundradius, 0.0f, 1.0f)*soundvol); // simple mono distance attenuation
        }
        if(stereo && (v.x != 0 || v.y != 0) && dist>0)
        {
            float yaw = -atan2f(v.x, v.y) - camera1->yaw*RAD; // relative angle of sound along X-Y axis
            pan = int(255.9f*(0.5f*sinf(yaw)+0.5f)); // range is from 0 (left) to 255 (right)
        }
    }
    vol = (vol*MAXVOL*chan.slot->volume)/255/255;
    vol = min(vol, MAXVOL);
    if(vol == chan.volume && pan == chan.pan) return false;
    chan.volume = vol;
    chan.pan = pan;
    chan.dirty = true;
    return true;
}  

void updatevol()
{
    updatemumble();
    if(nosound) return;
    int dirty = 0;
    loopv(channels)
    {
        soundchannel &chan = channels[i];
        if(chan.inuse && chan.hasloc() && updatechannel(chan)) dirty++;
    }
    if(dirty)
    {
        SDL_LockAudio(); // workaround for race conditions inside Mix_SetPanning
        loopv(channels) 
        {
            soundchannel &chan = channels[i];
            if(chan.inuse && chan.dirty) syncchannel(chan);
        }
        SDL_UnlockAudio();
    }
    if(mod && !Mix_PlayingMusic()) musicdone();
}

VARP(maxsoundsatonce, 0, 5, 100);

VAR(dbgsound, 0, 0, 1);

int playsound(int n, const vec *loc, extentity *ent, int loops, int fade, int chanid)
{
    if(nosound) return -1;
    if(!soundvol) return -1;

    if(chanid < 0 && !ent)
    {
        if(loc && maxsoundradius && camera1->o.dist(*loc) > 1.5f*maxsoundradius) return -1; // skip sounds that are unlikely to be heard
        static int soundsatonce = 0, lastsoundmillis = 0;
        if(totalmillis==lastsoundmillis) soundsatonce++; else soundsatonce = 1;
        lastsoundmillis = totalmillis;
        if(maxsoundsatonce && soundsatonce>maxsoundsatonce) return -1;  // avoid bursts of sounds with heavy packetloss and in sp
    }

    vector<soundslot> &sounds = ent ? mapsounds : gamesounds;
    if(!sounds.inrange(n)) { conoutf(CON_WARN, "unregistered sound: %d", n); return -1; }
    soundslot &slot = sounds[n];
    if(ent && slot.maxuses && slot.uses>=slot.maxuses) return -1;

    if(!slot.sample->chunk)
    {
        const char *exts[] = { "", ".wav", ".ogg" };
        string buf;
        loopi(sizeof(exts)/sizeof(exts[0]))
        {
            s_sprintf(buf)("packages/sounds/%s%s", slot.sample->name, exts[i]);
            const char *file = findfile(path(buf), "rb");
            slot.sample->chunk = Mix_LoadWAV(file);
            if(slot.sample->chunk) break;
        }

        if(!slot.sample->chunk) { conoutf(CON_ERROR, "failed to load sample: %s", buf); return -1; }
    }

    if(channels.inrange(chanid))
    {
        soundchannel &chan = channels[chanid];
        if(chan.inuse && chan.slot == &slot) 
        {
            if(loc) chan.loc = *loc;
            return chanid;
        }
    }
    if(fade < 0) return -1;
           
    if(dbgsound) conoutf("sound: %s", slot.sample->name);
 
    chanid = -1;
    loopv(channels) if(!channels[i].inuse) { chanid = i; break; }
    if(chanid < 0 && channels.length() < maxchannels) chanid = channels.length();
    if(chanid < 0) return -1;

    SDL_LockAudio(); // must lock here to prevent freechannel/Mix_SetPanning race conditions
    soundchannel &chan = newchannel(chanid, &slot, loc, ent);
    updatechannel(chan);
    if(fade) 
    {
        Mix_Volume(chanid, chan.volume);
        chanid = Mix_FadeInChannel(chanid, slot.sample->chunk, loops, fade);
    }
    else chanid = Mix_PlayChannel(chanid, slot.sample->chunk, loops);
    if(chanid >= 0) syncchannel(chan); 
    else freechannel(chanid);
    SDL_UnlockAudio();
    return chanid;
}

bool stopsound(int n, int chanid, int fade)
{
    if(!channels.inrange(chanid) || !channels[chanid].inuse || !gamesounds.inrange(n) || channels[chanid].slot != &gamesounds[n]) return false;
    if(dbgsound) conoutf("stopsound: %s", channels[chanid].slot->sample->name);
    if(!fade || !Mix_FadeOutChannel(chanid, fade))
    {
        Mix_HaltChannel(chanid);
        freechannel(chanid);
    }
    return true;
}

int playsoundname(const char *s, const vec *loc, int vol, int loops, int fade, int chanid) 
{ 
    if(!vol) vol = 100;
    int id = findsound(s, vol, gamesounds);
    if(id < 0) id = addsound(s, vol, 0, gamesounds);
    return playsound(id, loc, NULL, loops, fade, chanid);
}

void sound(int *n) { playsound(*n); }
COMMAND(sound, "i");

void resetsound()
{
    const SDL_version *v = Mix_Linked_Version();
    if(SDL_VERSIONNUM(v->major, v->minor, v->patch) <= SDL_VERSIONNUM(1, 2, 8))
    {
        conoutf(CON_ERROR, "Sound reset not available in-game due to SDL_mixer-1.2.8 bug. Please restart for changes to take effect.");
        return;
    }
    clearchanges(CHANGE_SOUND);
    if(!nosound) 
    {
        enumerate(samples, soundsample, s, { Mix_FreeChunk(s.chunk); s.chunk = NULL; });
        if(mod)
        {
            Mix_HaltMusic();
            Mix_FreeMusic(mod);
        }
        Mix_CloseAudio();
    }
    initsound();
    resetchannels();
    if(nosound)
    {
        DELETEA(musicfile);
        DELETEA(musicdonecmd);
        mod = NULL;
        gamesounds.setsizenodelete(0);
        mapsounds.setsizenodelete(0);
        samples.clear();
        return;
    }
    if(mod && (mod = Mix_LoadMUS(musicfile)))
    {
        Mix_PlayMusic(mod, musicdonecmd[0] ? 0 : -1);
        Mix_VolumeMusic((musicvol*MAXVOL)/255);
    }
}

COMMAND(resetsound, "");

#ifdef WIN32

#include <wchar.h>

#else

#include <unistd.h>

#ifdef _POSIX_SHARED_MEMORY_OBJECTS
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <wchar.h>
#endif

#endif

#if defined(WIN32) || defined(_POSIX_SHARED_MEMORY_OBJECTS)
struct MumbleInfo
{
    int version, timestamp;
    vec pos, front, top;
    wchar_t name[256];
};
#endif

#ifdef WIN32
static HANDLE mumblelink = NULL;
static MumbleInfo *mumbleinfo = NULL;
#define VALID_MUMBLELINK (mumblelink && mumbleinfo)
#elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
static int mumblelink = -1;
static MumbleInfo *mumbleinfo = (MumbleInfo *)-1; 
#define VALID_MUMBLELINK (mumblelink >= 0 && mumbleinfo != (MumbleInfo *)-1)
#endif

#ifdef VALID_MUMBLELINK
VARFP(mumble, 0, 1, 1, { if(mumble) initmumble(); else closemumble(); });
#else
VARFP(mumble, 0, 0, 1, { if(mumble) initmumble(); else closemumble(); });
#endif

void initmumble()
{
    if(!mumble) return;
#ifdef VALID_MUMBLELINK
    if(VALID_MUMBLELINK) return;

    #ifdef WIN32
        mumblelink = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "MumbleLink");
        if(mumblelink)
        {
            mumbleinfo = (MumbleInfo *)MapViewOfFile(mumblelink, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MumbleInfo));
            if(mumbleinfo) wcsncpy(mumbleinfo->name, L"Sauerbraten", 256);
        }
    #elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
        s_sprintfd(shmname)("/MumbleLink.%d", getuid());
        mumblelink = shm_open(shmname, O_RDWR, 0);
        if(mumblelink >= 0)
        {
            mumbleinfo = (MumbleInfo *)mmap(NULL, sizeof(MumbleInfo), PROT_READ|PROT_WRITE, MAP_SHARED, mumblelink, 0);
            if(mumbleinfo != (MumbleInfo *)-1) wcsncpy(mumbleinfo->name, L"Sauerbraten", 256);
        }
    #endif
    if(!VALID_MUMBLELINK) closemumble();
#else
    conoutf(CON_ERROR, "Mumble positional audio is not available on this platform.");
#endif
}

void closemumble()
{
#ifdef WIN32
    if(mumbleinfo) { UnmapViewOfFile(mumbleinfo); mumbleinfo = NULL; }
    if(mumblelink) { CloseHandle(mumblelink); mumblelink = NULL; }
#elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
    if(mumbleinfo != (MumbleInfo *)-1) { munmap(mumbleinfo, sizeof(MumbleInfo)); mumbleinfo = (MumbleInfo *)-1; } 
    if(mumblelink >= 0) { close(mumblelink); mumblelink = -1; }
#endif
}

static inline vec mumblevec(const vec &v, bool pos = false)
{
    // change from Z up, -Y forward to Y up, +Z forward
    // 8 cube units = 1 meter
    vec m(v.x, v.z, -v.y);
    if(pos) m.div(8);
    return m;
}

void updatemumble()
{
#ifdef VALID_MUMBLELINK
    if(!VALID_MUMBLELINK) return;

    static int timestamp = 0;

    mumbleinfo->version = 1;
    mumbleinfo->timestamp = ++timestamp;

    mumbleinfo->pos = mumblevec(player->o, true);
    mumbleinfo->front = mumblevec(vec(RAD*player->yaw, RAD*player->pitch));
    mumbleinfo->top = mumblevec(vec(RAD*player->yaw, RAD*(player->pitch+90)));
#endif
}

