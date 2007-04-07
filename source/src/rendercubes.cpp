// rendercubes.cpp: sits in between worldrender.cpp and rendergl.cpp and fills the vertex array for different cube surfaces.

#include "cube.h"

vector<vertex> verts;

void finishstrips();

#ifdef __APPLE__
    #define GL_COMBINE_EXT GL_COMBINE_ARB
    #define GL_COMBINE_RGB_EXT GL_COMBINE_RGB_ARB
    #define GL_SOURCE0_RGB_EXT GL_SOURCE0_RGB_ARB
    #define GL_SOURCE1_RGB_EXT GL_SOURCE1_RGB_ARB
    #define GL_RGB_SCALE_EXT GL_RGB_SCALE_ARB
    #define GL_PRIMARY_COLOR_EXT GL_PRIMARY_COLOR_ARB
#endif

void setupstrips()
{
    finishstrips();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    vertex *buf = verts.getbuf();
    glVertexPointer(3, GL_FLOAT, sizeof(vertex), &buf->x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex), &buf->r);
    glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), &buf->u);

    if(hasoverbright)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
    }
}

struct strip { int type, start, num; };

struct stripbatch
{
    int tex;
    vector<strip> strips;
};

vector<strip> skystrips;
stripbatch stripbatches[256];
uchar renderedtex[256];
int renderedtexs = 0;

void renderstripssky()
{
    if(skystrips.empty()) return;
    int xs, ys;
    glBindTexture(GL_TEXTURE_2D, lookuptexture(DEFAULT_SKY, xs, ys));
    loopv(skystrips) glDrawArrays(skystrips[i].type, skystrips[i].start, skystrips[i].num);
    skystrips.setsizenodelete(0);
}

void renderstrips()
{
    int xs, ys;
    loopj(renderedtexs)
    {
        stripbatch &sb = stripbatches[j];
        glBindTexture(GL_TEXTURE_2D, lookuptexture(sb.tex, xs, ys));
        loopv(sb.strips) glDrawArrays(sb.strips[i].type, sb.strips[i].start, sb.strips[i].num);
        sb.strips.setsizenodelete(0);
    }
    renderedtexs = 0;
}

void addstrip(int type, int tex, int start, int n)
{
    vector<strip> *strips;

    if(tex==DEFAULT_SKY)
    {
        if(minimap) return;
        strips = &skystrips;
    }
    else
    {
        stripbatch *sb = &stripbatches[renderedtex[tex]];
        if(sb->tex!=tex || sb>=&stripbatches[renderedtexs])
        {
            sb = &stripbatches[renderedtex[tex] = renderedtexs++];
            sb->tex = tex;
        }
        strips = &sb->strips;
    }
    if(type!=GL_TRIANGLE_STRIP && !strips->empty())
    {
        strip &last = strips->last();
        if(last.type==type && last.start+last.num==start)
        {
            last.num += n;
            return;
        }
    }
    strip &s = strips->add();
    s.type = type;
    s.start = start;
    s.num = n;
}

// generating the actual vertices is done dynamically every frame and sits at the
// leaves of all these functions, and are part of the cpu bottleneck on really slow
// machines, hence the macros.

#define vert(v1, v2, v3, ls, t1, t2) { \
	vertex &v = verts.add(); \
    v.u = t1; v.v = t2; \
    v.x = (float)(v1); v.y = (float)(v2); v.z = (float)(v3); \
    v.r = ls->r; v.g = ls->g; v.b = ls->b; v.a = 255; \
}

int nquads;
const float TEXTURESCALE = 32.0f;
bool floorstrip = false, deltastrip = false;
int oh, oy, ox, striptex;                         // the o* vars are used by the stripification
int ol3r, ol3g, ol3b, ol4r, ol4g, ol4b;      
int firstindex;
bool showm = false;

void showmip() { showm = !showm; }
void mipstats(int a, int b, int c) { if(showm) conoutf("1x1/2x2/4x4: %d / %d / %d", a, b, c); }

COMMAND(showmip, ARG_NONE);

VAR(mergestrips, 0, 1, 1);

#define stripend() \
    if(floorstrip || deltastrip) { \
        int type = GL_TRIANGLE_STRIP, len = verts.length()-firstindex; \
        if(mergestrips) switch(len) { \
            case 3: type = GL_TRIANGLES; break; \
            case 4: type = GL_QUADS; swap(vertex, verts.last(), verts[verts.length()-2]); break; \
         } \
         addstrip(type, striptex, firstindex, len); \
         floorstrip = deltastrip = false; \
    }

void finishstrips() { stripend(); }

sqr sbright, sdark;
VARP(lighterror,1,8,100);

void render_flat(int wtex, int x, int y, int size, int h, sqr *l1, sqr *l2, sqr *l3, sqr *l4, bool isceil)  // floor/ceil quads
{
    if(showm) { l3 = l1 = &sbright; l4 = l2 = &sdark; }

    int sx, sy;
    lookuptexture(wtex, sx, sy);
    float xf = TEXTURESCALE/sx;
    float yf = TEXTURESCALE/sy;
    float xs = size*xf;
    float ys = size*yf;
    float xo = xf*x;
    float yo = yf*y;

    bool first = !floorstrip || y!=oy+size || striptex!=wtex || h!=oh || x!=ox;

    if(first)       // start strip here
    {
        stripend();
        firstindex = verts.length();
        striptex = wtex;
        oh = h;
        ox = x;
        floorstrip = true;
        if(isceil)
        {
            vert(x+size, h, y, l2, xo+xs, yo);
            vert(x,      h, y, l1, xo, yo);
        }
        else
        {
            vert(x,      h, y, l1, xo,    yo);
            vert(x+size, h, y, l2, xo+xs, yo);
        }
        ol3r = l1->r;
        ol3g = l1->g;
        ol3b = l1->b;
        ol4r = l2->r;
        ol4g = l2->g;
        ol4b = l2->b;
    }
    else        // continue strip
    {
        int lighterr = lighterror*2;
        if((abs(ol3r-l3->r)<lighterr && abs(ol4r-l4->r)<lighterr        // skip vertices if light values are close enough
        &&  abs(ol3g-l3->g)<lighterr && abs(ol4g-l4->g)<lighterr
        &&  abs(ol3b-l3->b)<lighterr && abs(ol4b-l4->b)<lighterr) || !wtex)   
        {
            verts.setsizenodelete(verts.length()-2);
            nquads--;
        }
        else
        {
            uchar *p3 = (uchar *)(&verts[verts.length()-1].r);
            ol3r = p3[0];  
            ol3g = p3[1];  
            ol3b = p3[2];
            uchar *p4 = (uchar *)(&verts[verts.length()-2].r);  
            ol4r = p4[0];
            ol4g = p4[1];
            ol4b = p4[2];
        }
    }

    if(isceil)
    {
        vert(x+size, h, y+size, l3, xo+xs, yo+ys);
        vert(x,      h, y+size, l4, xo,    yo+ys); 
    }
    else
    {
        vert(x,      h, y+size, l4, xo,    yo+ys);
        vert(x+size, h, y+size, l3, xo+xs, yo+ys); 
    }

    oy = y;
    nquads++;
}

void render_flatdelta(int wtex, int x, int y, int size, float h1, float h2, float h3, float h4, sqr *l1, sqr *l2, sqr *l3, sqr *l4, bool isceil)  // floor/ceil quads on a slope
{
    if(showm) { l3 = l1 = &sbright; l4 = l2 = &sdark; }

    int sx, sy;
    lookuptexture(wtex, sx, sy);
    float xf = TEXTURESCALE/sx;
    float yf = TEXTURESCALE/sy;
    float xs = size*xf;
    float ys = size*yf;
    float xo = xf*x;
    float yo = yf*y;

    bool first = !deltastrip || y!=oy+size || striptex!=wtex || x!=ox; 

    if(first) 
    {
        stripend();
        firstindex = verts.length();
        striptex = wtex;
        ox = x;
        deltastrip = true;
        if(isceil)
        {
            vert(x+size, h2, y, l2, xo+xs, yo);
            vert(x,      h1, y, l1, xo,    yo);
        }
        else
        {
            vert(x,      h1, y, l1, xo,    yo);
            vert(x+size, h2, y, l2, xo+xs, yo);
        }
        ol3r = l1->r;
        ol3g = l1->g;
        ol3b = l1->b;
        ol4r = l2->r;
        ol4g = l2->g;
        ol4b = l2->b;
    }

    if(isceil)
    {
        vert(x+size, h3, y+size, l3, xo+xs, yo+ys); 
        vert(x,      h4, y+size, l4, xo,    yo+ys);
    }
    else
    {
        vert(x,      h4, y+size, l4, xo,    yo+ys);
        vert(x+size, h3, y+size, l3, xo+xs, yo+ys); 
    }

    oy = y;
    nquads++;
}

void render_2tris(sqr *h, sqr *s, int x1, int y1, int x2, int y2, int x3, int y3, sqr *l1, sqr *l2, sqr *l3)   // floor/ceil tris on a corner cube
{
    stripend();

    int sx, sy;
    lookuptexture(h->ftex, sx, sy);
    float xf = TEXTURESCALE/sx;
    float yf = TEXTURESCALE/sy;

    vert(x1, h->floor, y1, l1, xf*x1, yf*y1);
    vert(x2, h->floor, y2, l2, xf*x2, yf*y2);
    vert(x3, h->floor, y3, l3, xf*x3, yf*y3);
    addstrip(mergestrips ? GL_TRIANGLES : GL_TRIANGLE_STRIP, h->ftex, verts.length()-3, 3);

    lookuptexture(h->ctex, sx, sy);
    xf = TEXTURESCALE/sx;
    yf = TEXTURESCALE/sy;

    vert(x3, h->ceil, y3, l3, xf*x3, yf*y3);
    vert(x2, h->ceil, y2, l2, xf*x2, yf*y2);
    vert(x1, h->ceil, y1, l1, xf*x1, yf*y1);
    addstrip(mergestrips ? GL_TRIANGLES : GL_TRIANGLE_STRIP, h->ctex, verts.length()-3, 3);
    nquads++;
}

void render_tris(int x, int y, int size, bool topleft,
                 sqr *h1, sqr *h2, sqr *s, sqr *t, sqr *u, sqr *v)
{
    if(topleft)
    {
        if(h1) render_2tris(h1, s, x+size, y+size, x, y+size, x, y, u, v, s);
        if(h2) render_2tris(h2, s, x, y, x+size, y, x+size, y+size, s, t, v);
    }
    else
    {
        if(h1) render_2tris(h1, s, x, y, x+size, y, x, y+size, s, t, u);
        if(h2) render_2tris(h2, s, x+size, y, x+size, y+size, x, y+size, t, u, v);
    }
}

void render_square(int wtex, float floor1, float floor2, float ceil1, float ceil2, int x1, int y1, int x2, int y2, int size, sqr *l1, sqr *l2, bool flip)   // wall quads
{
    stripend();
    if(showm) { l1 = &sbright; l2 = &sdark; }

    int sx, sy;
    lookuptexture(wtex, sx, sy);
    float xf = TEXTURESCALE/sx;
    float yf = TEXTURESCALE/sy;
    float xs = size*xf;
    float xo = xf*(x1==x2 ? min(y1,y2) : min(x1,x2));

    if(!flip)
    {
        vert(x2, ceil2,  y2, l2, xo+xs, -yf*ceil2);
        vert(x1, ceil1,  y1, l1, xo,    -yf*ceil1);
        if(mergestrips) vert(x1, floor1, y1, l1, xo, -floor1*yf);
        vert(x2, floor2, y2, l2, xo+xs, -floor2*yf);
        if(!mergestrips) vert(x1, floor1, y1, l1, xo, -floor1*yf);
    }
    else
    {
        vert(x1, ceil1,  y1, l1, xo,    -yf*ceil1);
        vert(x2, ceil2,  y2, l2, xo+xs, -yf*ceil2);
        if(mergestrips) vert(x2, floor2, y2, l2, xo+xs, -floor2*yf);
        vert(x1, floor1, y1, l1, xo,    -floor1*yf);
        if(!mergestrips) vert(x2, floor2, y2, l2, xo+xs, -floor2*yf);
    }
    addstrip(mergestrips ? GL_QUADS : GL_TRIANGLE_STRIP, wtex, verts.length()-4, 4);
    nquads++;
}

int wx1, wy1, wx2, wy2;

VARP(watersubdiv, 1, 4, 64);
VARF(waterlevel, -128, -128, 127, if(!noteditmode()) hdr.waterlevel = waterlevel);

void setwatercolor(char *r, char *g, char *b, char *a)
{
    if(r[0])
    {
        hdr.watercolor[0] = ATOI(r);
        hdr.watercolor[1] = ATOI(g);
        hdr.watercolor[2] = ATOI(b);
        hdr.watercolor[3] = a[0] ? ATOI(a) : 178;
    }
    else
    {
        hdr.watercolor[0] = 25;
        hdr.watercolor[1] = 76;
        hdr.watercolor[2] = 102;
        hdr.watercolor[3] = 178;
    }
}

COMMANDN(watercolour, setwatercolor, ARG_4STR);

inline void vertw(int v1, float v2, int v3, float t, int tex)
{
    float angle = v1*v3*0.1f + t;
    float h = sinf(angle);
    if(tex)
    {
        float v = cosf(angle);
        if(tex>0)
        {
            glTexCoord3f(v1+v*0.3f, v2+h*0.3f, v3+v*0.3f);
            if(tex>1) glColor4f(1, 1, 1, 0.15f + max(v, 0)*0.15f);
        }
        else glColor4ub(hdr.watercolor[0], hdr.watercolor[1], hdr.watercolor[2], (uchar)(hdr.watercolor[3] + (max(v, 0) - 0.5f)*51.0f));
    }
    glVertex3f((float)v1, v2 + h*0.3f, (float)v3);
}

// renders water for bounding rect area that contains water... simple but very inefficient

void renderwaterstrips(float hf, int tex, float t)
{
    for(int x = wx1; x<wx2; x += watersubdiv)
    {
        glBegin(GL_TRIANGLE_STRIP);
        vertw(x,             hf, wy1, t, tex);
        vertw(x+watersubdiv, hf, wy1, t, tex);
        for(int y = wy1; y<wy2; y += watersubdiv)
        {
            vertw(x,             hf, y+watersubdiv, t, tex);
            vertw(x+watersubdiv, hf, y+watersubdiv, t, tex);
        }
        glEnd();
    }
}

int renderwater(float hf, GLuint reflecttex, GLuint refracttex)
{
    if(wx1<0) return nquads;

    wx1 &= ~(watersubdiv-1);
    wy1 &= ~(watersubdiv-1);

    float t = lastmillis/300.0f;

    if(!refracttex) 
    {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if(reflecttex)
    {
        if(!refracttex)
        {
            glColor4ubv(hdr.watercolor);
            renderwaterstrips(hf, 0, t);

            glEnable(GL_TEXTURE_2D);
        }

        GLfloat pm[16], mm[16];
        glGetFloatv(GL_PROJECTION_MATRIX, pm);
        glGetFloatv(GL_MODELVIEW_MATRIX, mm);

        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glTranslatef(0.5f, 0.5f, 0.5f);
        glScalef(0.5f, 0.5f, 0.5f);
        glMultMatrixf(pm);
        glMultMatrixf(mm);

        glBindTexture(GL_TEXTURE_2D, refracttex ? refracttex : reflecttex);
    }

    if(refracttex) 
    {
        glColor3f(1, 1, 1);
        renderwaterstrips(hf, 1, t);
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);

        glDepthFunc(GL_LEQUAL);
        glBindTexture(GL_TEXTURE_2D, reflecttex);
    }
    renderwaterstrips(hf, reflecttex ? 2 : -1, t);

    if(reflecttex)
    {
        if(refracttex) glDepthFunc(GL_LESS);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
    }
    else glEnable(GL_TEXTURE_2D);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
   
    return nquads + (reflecttex ? nquads : 0);
}

void addwaterquad(int x, int y, int size)       // update bounding rect that contains water
{
    int x2 = x+size;
    int y2 = y+size;
    if(wx1<0)
    {
        wx1 = x;
        wy1 = y;
        wx2 = x2;
        wy2 = y2;
    }
    else
    {
        if(x<wx1) wx1 = x;
        if(y<wy1) wy1 = y;
        if(x2>wx2) wx2 = x2;
        if(y2>wy2) wy2 = y2;
    }
}

void resetcubes()
{
    verts.setsizenodelete(0);

    floorstrip = deltastrip = false;
    if(!reflecting) wx1 = -1;
    nquads = 0;
    sbright.r = sbright.g = sbright.b = 255;
    sdark.r = sdark.g = sdark.b = 0;
}


