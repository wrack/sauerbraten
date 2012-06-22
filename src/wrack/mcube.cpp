#include "engine.h"
#include "mcube.h"

extern selinfo sel;
extern void changed(const block3 &sel, bool commit = true);

///////// selection support from octaedit /////////////
extern cube  &blockcube(int x, int y, int z, const block3 &b, int rgrid);
#define loopxy(b)        loop(y,(b).s[C[dimension((b).orient)]]) loop(x,(b).s[R[dimension((b).orient)]])
#define loopxyz(b, r, f) { loop(z,(b).s[D[dimension((b).orient)]]) loopxy((b)) { cube &c = blockcube(x,y,z,b,r); f; } }
#define loopselxyz(f)    { makeundo(); loopxyz(sel, sel.grid, f); changed(sel); }
#define selcube(x, y, z) blockcube(x, y, z, sel, sel.grid)
///////// selection support from octaedit end/////////////

//- select bottom face for loopxyz, so that x y z is correct x y z [cube] in block3, change sel
#define finish_geom(iso_geom) iso_geom->aabb.orient = 4; loopxyz(iso_geom->aabb, iso_geom->aabb.grid, march_cube(c, x, y, z, iso_geom->aabb.grid,iso_geom, *d) ); changed(sel);

void buildsphere(float *r,int *d,float *ox,float *oy,float *oz,float *sx,float *sy,float *sz)
{
	iso* iso_geom = &sphere_iso(sel,*r,vec(*ox,*oy,*oz),vec(*sx,*sy,*sz));
	finish_geom(iso_geom);
}
COMMAND(buildsphere, "fiffffff");

void buildhelix(float *r,float *br,int *d)
{
	iso* iso_geom = &helix_iso(sel,*br,*r);
	finish_geom(iso_geom);
}
COMMAND(buildhelix, "ffi");

void buildcylinder(float *r,float *h,int *d,float *ox,float *oy,float *oz)
{
	iso* iso_geom = &cylinder_iso(sel,*r,*h,vec(*ox,*oy,*oz));
	finish_geom(iso_geom);
}
COMMAND(buildcylinder, "ffifff");

void buildparaboloid(float *r,int *d,float *ox,float *oy,float *oz)
{
	iso* iso_geom = &paraboloid_iso(sel,*r,vec(*ox,*oy,*oz));
	finish_geom(iso_geom);
}
COMMAND(buildparaboloid, "fifff");

void buildoctahedron(float *r,int *d,float *ox,float *oy,float *oz)
{
	iso* iso_geom = &octahedron_iso(sel,*r,vec(*ox,*oy,*oz));
	finish_geom(iso_geom);
}
COMMAND(buildoctahedron, "fifff");

void buildci(unsigned int *ci)
{
	sel.orient = 4; //select bottom face for loopxyz, so that x y z is correct x y z [cube] in block3
	loopxyz(sel, sel.grid, march_cube(c,*ci) );	
	changed(sel);
}
COMMAND(buildci, "i");

void buildallci()
{
	sel.s = ivec(51,5,1);
	sel.orient = 4; //select bottom face for loopxyz, so that x y z is correct x y z [cube] in block3
	unsigned int ci = 0;
	loopxyz(sel, sel.grid, march_cube(c,ci); ci++; );
	changed(sel);
}
COMMAND(buildallci, "");

// IMPORT *.smc (Sauerbraten Marching Cubes)

void importsmc(char *name,bool *voxel)
{
	int cnt = 0;
	stream *f = openfile(name, "r");
	if(!f) {
		conoutf("No such File");
		return;
	}
	char buf[1024];
	int x,y,z,g;
	double d1,d2,d3,d4,d5,d6,d7,d8;
	march_cube *mc;
    while(f->getline(buf, sizeof(buf)))
	{

		cnt++;
        sscanf(buf, "%i %i %i %i %lf %lf %lf %lf %lf %lf %lf %lf", &x,&y,&z,&g,&d1,&d2,&d3,&d4,&d5,&d6,&d7,&d8);
		if(*voxel)
		{
			solidfaces(lookupcube(sel.o.x+x, sel.o.y+y, sel.o.z+z, g));
		}
		else
		{
			mc = new march_cube(lookupcube(sel.o.x+x, sel.o.y+y, sel.o.z+z, g));
			mc->corners[0] = iso_point(d1);
			mc->corners[1] = iso_point(d2);
			mc->corners[2] = iso_point(d4);
			mc->corners[3] = iso_point(d3);
			mc->corners[4] = iso_point(d5);
			mc->corners[5] = iso_point(d6);
			mc->corners[6] = iso_point(d8);
			mc->corners[7] = iso_point(d7);

			mc->init_cube_index();
			mc->render();
		}
	}
	conoutf("cubes: %i",cnt);
	f->close();
	allchanged();
}
COMMAND(importsmc,"si");


//-- FOR DEBUG
void buildallcisorted()
{
	int sy=sel.o.y;
	loop(x,22){
		sel.o.y = sy;
		buildci(&cube_index_base_cases[x]);
		for(unsigned int ci=0;ci<255;ci++){
			if(cube_index_cases[ci]==cube_index_base_cases[x] && ci!=cube_index_base_cases[x]){
				sel.o.y += sel.grid;
				buildci(&ci);
			}
		}
		sel.o.x += sel.grid;
	}
}
COMMAND(buildallcisorted, "");

void getci()
{
	uchar nce[12];
	bool equal = true;
	loopxyz(sel, sel.grid, 
		loop(ci,255){
			loop(e,12){ nce[e] = c.edges[e]; }
			march_cube(c,ci);
			equal = true;
			loop(e,12){ if(c.edges[e] != nce[e]){ equal=false; } }
			loop(e,12){ c.edges[e] = nce[e]; }
			if(equal){ 
				conoutf("ci: %i[%i]",ci,cube_index_cases[ci]);
				ci=255;
			}
		}
	);
	changed(sel);
}
COMMAND(getci, "");

void getline(char *name){
	int l = 1;
	stream *f = openfile(name, "r");
	if(!f) {
		conoutf("No such File");
		return;
	}
	char buf[1024];
	int x,y,z,g;
	double d1,d2,d3,d4,d5,d6,d7,d8;
    while(f->getline(buf, sizeof(buf)))
	{
        sscanf(buf, "%i %i %i %i %lf %lf %lf %lf %lf %lf %lf %lf", &x,&y,&z,&g,&d1,&d2,&d3,&d4,&d5,&d6,&d7,&d8);
		if(x==sel.o.x && y==sel.o.y && z==sel.o.z){
			conoutf("%i: %lf %lf %lf %lf %lf %lf %lf %lf",l, d1,d2,d3,d4,d5,d6,d7,d8);
		}
		l++;
	}
	f->close();
}
COMMAND(getline,"");
//- FOR DEBUG, end
