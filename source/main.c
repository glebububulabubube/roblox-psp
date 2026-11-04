#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SCR_W 480
#define SCR_H 272
#define MAXVERTS 9000

PSP_MODULE_INFO("Roblox PSP", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

static int exitRequest = 0;

typedef struct { float x,y,z; unsigned int color; } Vertex;

static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned int __attribute__((aligned(16))) displayBuf[2][SCR_W*SCR_H];
static unsigned int __attribute__((aligned(16))) depthBuf[SCR_W*SCR_H];
static Vertex __attribute__((aligned(16))) g_verts[MAXVERTS];

static unsigned int colors[] = {
  0xFFA0A0A0, 0xFF8B5A2B, 0xFF2E8B57, 0xFF3060C8, 0xFF909090,
  0xFFE0B000, 0xFFD08030, 0xFFC03030, 0xFF9040C8, 0xFF30C0C0,
};

#define WORLD_X 16
#define WORLD_Y 16
#define WORLD_Z 16
#define MAXBLOCKS 2048
#define NUM_PLACES 4

typedef struct { int x,y,z; int col; } Block;
static Block blocks[MAXBLOCKS];
static int nblocks = 0;
static int place = 0;
static const char* placeNames[NUM_PLACES] = {"BASE","FOREST","DESERT","ICE"};

static float px=2.5f, py=4.0f, pz=2.5f;
static float vy=0;
static float camYaw=0, camPitch=0.3f, camDist=6.5f;
static int onGround=0;
static float speed=0.10f;
static float gravity=0.004f;

static void exitCallback(void){ exitRequest=1; }
static void initCallbacks(void){
    int id = sceKernelCreateCallback("exit", exitCallback, NULL);
    sceKernelRegisterExitCallback(id);
    atexit(sceKernelExitGame);
}

static void initGraphics(void){
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, displayBuf[0], SCR_W);
    sceGuDispBuffer(SCR_W, SCR_H, displayBuf[1], SCR_W);
    sceGuDepthBuffer(depthBuf, SCR_W);
    sceGuOffset(2048-(SCR_W/2), 2048-(SCR_H/2));
    sceGuViewport(2048,2048,SCR_W,SCR_H);
    sceGuDepthRange(0xc350,0x2710);
    sceGuScissor(0,0,SCR_W,SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuShadeModel(GU_FLAT);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void addBlock(int x,int y,int z,int col){
    if(nblocks>=MAXBLOCKS) return;
    if(x<0||x>=WORLD_X||y<0||y>=WORLD_Y||z<0||z>=WORLD_Z) return;
    blocks[nblocks].x=x; blocks[nblocks].y=y; blocks[nblocks].z=z; blocks[nblocks].col=col;
    nblocks++;
}

static void buildWorld(int pid){
    nblocks=0;
    int ground=(pid==1?1:pid==2?5:pid==3?9:3);
    for(int x=0;x<WORLD_X;x++)
        for(int z=0;z<WORLD_Z;z++)
            addBlock(x,0,z,ground);
    if(pid==0){
        for(int y=1;y<=3;y++){ addBlock(2,y,2,4); addBlock(13,y,13,4); addBlock(2,y,13,4); addBlock(13,y,2,4); }
    } else if(pid==1){
        int t[14][3]={{4,1,4},{4,2,4},{4,3,4},{4,4,4},{3,4,4},{5,4,4},{4,4,3},{4,4,5},{11,1,11},{11,2,11},{11,3,11},{7,4,11},{8,4,12},{6,4,12}};
        for(int i=0;i<14;i++) addBlock(t[i][0],t[i][1],t[i][2],(t[i][1]==4)?1:2);
    } else if(pid==2){
        for(int y=1;y<=4;y++)
            for(int dx=-4+y;dx<=4-y;dx++)
                for(int dz=-4+y;dz<=4-y;dz++)
                    addBlock(8+dx,y,8+dz,5);
    } else {
        for(int y=1;y<=4;y++){ addBlock(8,y,8,9); addBlock(5,y,11,9); addBlock(11,y,5,9); }
    }
    addBlock(7,1,15,8); addBlock(8,1,15,8);
}

static unsigned int shade(unsigned int c,float f){
    unsigned r=(c>>16)&0xff,g=(c>>8)&0xff,b=(c)&0xff;
    r=(unsigned)(r*f); g=(unsigned)(g*f); b=(unsigned)(b*f);
    if(r>255)r=255; if(g>255)g=255; if(b>255)b=255;
    return 0xFF000000|(r<<16)|(g<<8)|b;
}

static int pushCube(int start,float bx,float by,float bz,unsigned int col){
    float x0=bx,y0=by,z0=bz,x1=bx+1,y1=by+1,z1=bz+1;
    float sh[6]={0.75f,0.6f,1.0f,0.5f,0.7f,0.55f};
    float fs[6][4][3]={
        {{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}},
        {{x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0}},
        {{x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1}},
        {{x0,y0,z1},{x1,y0,z1},{x1,y0,z0},{x0,y0,z0}},
        {{x1,y0,z0},{x1,y0,z1},{x1,y1,z1},{x1,y1,z0}},
        {{x0,y0,z1},{x0,y0,z0},{x0,y1,z0},{x0,y1,z1}},
    };
    int inx=start;
    for(int f=0;f<6;f++){
        unsigned cc=shade(col,sh[f]);
        unsigned order[6]={0,1,2,0,2,3};
        for(int k=0;k<6;k++){
            int oi=order[k];
            g_verts[inx].x=fs[f][oi][0]; g_verts[inx].y=fs[f][oi][1]; g_verts[inx].z=fs[f][oi][2];
            g_verts[inx].color=cc;
            inx++;
        }
    }
    return inx;
}

static void drawWorld(void){
    sceGuStart(GU_DIRECT,list);
    sceGuClearColor(0xFF305080);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_BLEND);
    float ex=px-sinf(camYaw)*cosf(camPitch)*camDist;
    float ey=py-sinf(camPitch)*camDist+1.5f;
    float ez=pz-cosf(camYaw)*cosf(camPitch)*camDist;
    if(ey<0.5f)ey=0.5f;
    ScePspFVector3 eye={ex,ey,ez}, center={px,py,pz}, up={0,1,0};
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(70.0f, SCR_W/(float)SCR_H, 0.5f, 200.0f);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumLookAt(&eye,&center,&up);
    int vn=0;
    for(int i=0;i<nblocks;i++)
        vn=pushCube(vn, (float)blocks[i].x,(float)blocks[i].y,(float)blocks[i].z, colors[blocks[i].col]);
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888|GU_VERTEX_32BITF, vn, 0, g_verts);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

int main(void){
    initCallbacks();
    initGraphics();
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    buildWorld(0);
    while(!exitRequest){
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad,1);
        float ax=(pad.Lx-128)/128.0f;
        float az=(pad.Ly-128)/128.0f;
        if(pad.Buttons&PSP_CTRL_UP) camPitch=fminf(camPitch+0.03f,1.2f);
        if(pad.Buttons&PSP_CTRL_DOWN) camPitch=fmaxf(camPitch-0.03f,-0.4f);
        if(pad.Buttons&PSP_CTRL_LEFT) camYaw-=0.035f;
        if(pad.Buttons&PSP_CTRL_RIGHT) camYaw+=0.035f;
        if(pad.Buttons&PSP_CTRL_CROSS && onGround){ vy=0.22f; onGround=0; }
        float s=sinf(camYaw), c=cosf(camYaw);
        float mvx=ax*c-az*s;
        float mvz=-ax*s-az*c;
        float ml=sqrtf(mvx*mvx+mvz*mvz);
        if(ml>1.0f){ mvx/=ml; mvz/=ml; }
        px+=mvx*speed; pz+=mvz*speed;
        vy-=gravity;
        py+=vy;
        if(py<1.0f){ py=1.0f; vy=0; onGround=1; }
        if(px<0.5f)px=0.5f; if(px>WORLD_X-0.5f)px=WORLD_X-0.5f;
        if(pz<0.5f)pz=0.5f; if(pz>WORLD_Z-0.5f)pz=WORLD_Z-0.5f;
        if(pz>WORLD_Z-1.0f){
            place=(place+1)%NUM_PLACES;
            buildWorld(place);
            pz=1.5f;
        }
        drawWorld();
    }
    sceGuTerm();
    return 0;
}
