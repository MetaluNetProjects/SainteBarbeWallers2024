// DMX512 "lamp"

#pragma once
#include "fraise.h"
#include <stdlib.h>

#define MAX_LAMPS 200
#define MAX_GROUPS 200

#ifndef CLIP
#define CLIP(x, min, max) (MIN(MAX(x, min), max))
#endif

class Lamp {
  private:
    static Lamp* lamps[MAX_LAMPS];
    static int* groups[MAX_GROUPS];
  protected:
    int id;
    int chan;
    Lamp(int _id, int _chan): id(_id), chan(_chan) { lamps[id] = this;}
    virtual void _compute(char *buf); // dest is the destination DMX buffer
    virtual void _command(const char *data, uint8_t len) {
        switch(data[0]) {
            case 200: // get chan
                printf("l lamp %d: chan %d\n", id, chan);
                break;
            default: ;
        }
    }
    inline void do_command(const char *data, uint8_t len) {
        _command(data, len);
        Lamp::_command(data, len);
    }
    static int period_ms; // compute period
  public:
    static void compute(char *buf) { 
        for(int i = 0; i < MAX_LAMPS; i++) { 
            if(lamps[i]) lamps[i]->_compute(buf);
        }
    }
    static void command(const char *data, uint8_t len) {
        if(!len) return;
        unsigned int i = data[0];
        data++; len--;
        if(len && i < MAX_LAMPS && lamps[i]) {
            /*lamps[i]->_command(data, len);
            lamps[i]->Lamp::_command(data, len);*/
            lamps[i]->do_command(data, len);
        }
    }
    static void config(const char *data, uint8_t len) {
        if(!len) return;
        unsigned int com = data[0];
        data++; len--;
        switch(com) {
            case 0: // period_ms
                period_ms = data[0];
                break;
            case 1: // define group
                if(len > 0) {
                    int g = data[0];
                    data++; len--;
                    if(g >= MAX_GROUPS) break;
                    if(groups[g]) free(groups[g]);
                    groups[g] = new int[len + 1];
                    int i = 0;
                    while(len--) {
                        groups[g][i] = data[i];
                        i++;
                    }
                    groups[g][i] = MAX_LAMPS; // MAX_LAMPS means 'end of group'
                }
                break;
            case 2: // group command
                if(len > 1) {
                    int g = data[0];
                    if(!groups[g]) { printf("e groups %d undefined!\n", g); return;}
                    data++; len--;
                    int i = 0;
                    int l = (unsigned int)groups[g][i];
                    while(l < MAX_LAMPS) {
                        if(lamps[l]) lamps[l]->do_command(data, len);
                        l = groups[g][++i];
                    }
                }
                break;
            default: ;
        }
        /*if(len && i < MAX_LAMPS && lamps[i]) {
            lamps[i]->do_command(data, len);
        }*/
    }
    static void set_period_ms(int ms) {period_ms = ms;}
};

class LampFade: public Lamp {
    float val = 0;
    unsigned char target = 0;
    float factor = 1.0; // ms
  public:
    LampFade(int id, int chan): Lamp(id, chan) {}
    void _compute(char *buf){
        val += ((float)target - val) * factor;
        buf[chan] = val;
    }
    void _command(const char *data, uint8_t len) {
        unsigned char command = data[0];
        unsigned int time_constant;
        switch(command) {
            case 0: // target
                target = data[1];
                break;
            case 1 : // time constant ms
                time_constant = data[1] * 256 + data[2];
                if(time_constant < 1) time_constant = 1;
                factor = ((float)period_ms) / time_constant;
                if(factor > 1.0) factor = 1.0;
                printf("factor: %f\n", factor);
                break;
            default: ;
        }
    }
};

inline int rnd(int scale) {
    return (int)((unsigned)random() % (unsigned)scale);
}

inline void filter(int &val, int input, int speed, int speed_scale) {
    val += ((input - val) * speed) / speed_scale;
}

inline void ffilter(float &val, int input, int speed, int speed_scale) {
    val += ((input - val) * speed) / (float)speed_scale;
}

class LampFire: public Lamp {
    int colorSpeedUp = 60, colorSpeedDown = 130;
    int colorMaxSpeedUp = 80, colorMaxSpeedDown = 160;
    int colorCountLow = 1, colorCountHigh = 1;
    int colorMaxCountLow = 1, colorMaxCountHigh = 1;
    int colorFiltFactor = 20; // percent
    int colorCount = 0;
    int colorFilt = 0, color = 0; // 0 - 65535
    int colorTarget = 30000;
    int colorRandom = 60; // percent

    int intensSpeed = 80; // 0 - 255
    int intensMax = 0; //255 * 16;
    int intensMin = 0;
    int intens = 0, intensFilt = 0; // 0 - 4095
    struct { unsigned char r, g, b, w; } colorLow = {150, 0, 0, 0}, colorHigh = {255, 240, 0, 0};
    float r, g, b, w;
    int man_speed = 50; // 0 - 255
    int flash_count;
    int flash_duration_ms = 700;
    enum {none, armed, done} flash_state = none;
    enum {low, up, high, down} col_state = up;
    unsigned char ncolors = 4;

    void recalc_params() {
        #define rndmix(val, percent) (rnd(val * percent) + val * (100 - percent))

        colorSpeedUp = rndmix(colorMaxSpeedUp, colorRandom); // colorSpeedUp ~= 100*colorMaxSpeedUp
        if(colorSpeedUp < 200) colorSpeedUp = 200;

        colorSpeedDown = rndmix(colorMaxSpeedDown, colorRandom);
        if(colorSpeedDown < 200) colorSpeedDown = 200;

        colorTarget = rndmix(65536, colorRandom) / 100;
        colorCountLow = rndmix(colorMaxCountLow, colorRandom) / 100;
        colorCountHigh = rndmix(colorMaxCountHigh, colorRandom) / 100;
    }

    inline void setbuf(char* buf, int chan, unsigned char r, unsigned char g, unsigned char b, unsigned char w) {
        buf[chan + 0] = r;
        if(ncolors > 1) {
            buf[chan + 1] = g;
            if(ncolors > 2) {
                buf[chan + 2] = b;
                if(ncolors > 3) buf[chan + 3] = w;
            }
        }
    }

    void _compute(char *buf){
        if(flash_state != none) {
            if(flash_state == armed) {
                if(flash_count == 0) {
                    setbuf(buf, chan, 255, 255, 255, 255);
                    flash_count = flash_duration_ms / period_ms;
                    flash_state = done;
                    return;
                }
                else flash_count--;
            }
            else if(flash_state == done) {
                float v = ((float)flash_count * period_ms) / flash_duration_ms;
                v = v * v * 255;
                v = CLIP(v, 0, 255);
                setbuf(buf, chan, v, v, v, v);
                if(flash_count == 0) flash_state = none;
                else flash_count--;
                return;
            }
            setbuf(buf, chan, 0, 0, 0, 0);
            intens = intensFilt = 0;
            return;
        }
        switch(col_state) {
            case up:
                color += colorSpeedUp;
                if(color > colorTarget) {
                    if(color > 65535) color = 65535;
                    recalc_params();
                    col_state = high;
                    colorCount = 0;
                }
                break;
            case high:
                if(colorCount >= colorCountHigh) {
                    col_state = down;
                } else colorCount++;
                break;
            case down:
                color -= colorSpeedDown;
                if(color < 0) {
                    color = 0;
                    col_state = low;
                    colorCount = 0;
                }
                break;
            case low:
                if(colorCount >= colorCountLow) {
                    col_state = up;
                } else colorCount++;
                break;
            default: ;
        }
        colorFilt += (color - colorFilt) * colorFiltFactor / 100;

        int intens_rnd = intensMax;
        if(intensMax > intensMin) intens_rnd = (1.5 * rnd(intensMax - intensMin)) + intensMin;
        intens += ((intens_rnd - intens) * intensSpeed) / 256;
        if(intens < 0) intens = 0;
        if(intens > 4095) intens = 4095;
        intensFilt += ((intens - intensFilt) * intensSpeed) / 256;

        #define MIX(c) ((((65535 - colorFilt) * colorLow.c + colorFilt * colorHigh.c) / 256) * intensFilt) / (4096 * 256);
        #define MIXCLIP(c) do{c = MIX(c); c = CLIP(c, 0, 255);} while(0)
        MIXCLIP(r);
        MIXCLIP(g);
        MIXCLIP(b);
        MIXCLIP(w);
        setbuf(buf, chan, r, g, b, w);
    }
    void _command(const char *data, uint8_t len) {
        unsigned char command = data[0];
        switch(command) {
            case 0: // intens
                intensMax = data[1] * 16;
                if(len > 2) intensMin = data[2] * 16;
                break;
            case 1: // config
                //if(len < 8) break;
                colorMaxSpeedUp     = data[1];
                colorMaxSpeedDown   = data[2];
                colorMaxCountLow    = data[3];
                colorMaxCountHigh   = data[4];
                colorRandom         = data[5];
                colorFiltFactor     = 100 - data[6];
                colorFiltFactor = CLIP(colorFiltFactor, 1, 100);
                intensSpeed         = data[7];
                if(intensSpeed < 2) intensSpeed = 2;
                break;
            case 2: // colors
                if(len < 9) break;
                colorLow.r  = data[1];
                colorLow.g  = data[2];
                colorLow.b  = data[3];
                colorLow.w  = data[4];
                colorHigh.r = data[5];
                colorHigh.g = data[6];
                colorHigh.b = data[7];
                colorHigh.w = data[8];
                break;
            case 10: //flash
                flash_state = armed;
                flash_count = rnd((data[1] * 10) / period_ms);  // random delay (x 10ms)
                flash_duration_ms = data[2] * 10;               // duration (x 10ms)
                break;
            case 100: // stats
                printf("l lamp %d intensFilt:%d\n", id, intensFilt);
                break;
            default: ;
        }
    }

  public:
    LampFire(int id, int chan, unsigned char ncolors): Lamp(id, chan), ncolors(ncolors) { ncolors = CLIP(ncolors, 1, 4);}
};
