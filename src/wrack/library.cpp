/// PWM Library by wrack ///
#include "engine.h"
//#include "game.h"
#include "library.h"

//-- Lib Items

//--- add,del

void libitem(const char *nam)
{
	library.add(iteminfo(sel,nam));
	conoutf("The selection is stored in the library under id %i.",library.length()-1);
}

void dellibitem(int *i)
{
	library.remove(*i);
}

COMMAND(libitem,"s");
COMMAND(dellibitem,"i");

//-- name set and get

void getitemname(int i,const char *name)
{
	if(strlen(library[i].name)==0)
	{ 
		formatstring(name)("libitem %i", i);
	}
	else
	{ 
		formatstring(name)("%s", library[i].name);
	}
}
void setlibitemname(int *i,const char *nam)
{
	library[*i].name = newstring(nam);
}
ICOMMAND(getlibitemname, "i", (int *i), string name; getitemname(*i,name); result(name); );
COMMAND(setlibitemname, "is");

//--- copy, paste, transformed-copy

void itemrotate(selinfo &itemsel,selinfo &trans,ivec &rot,bool back=false)
{
	if(back)
	{
		if(rot.x!=0){ trans.orient = 0; mprotate(-1,trans,true); selrotate(itemsel,trans,1,0); }
		if(rot.y!=0){ trans.orient = 2; mprotate(-1,trans,true); selrotate(itemsel,trans,1,1); }
		if(rot.z!=0){ trans.orient = 4; mprotate(-1,trans,true); selrotate(itemsel,trans,1,2); }
	}
	else
	{
		if(rot.x!=0){ trans.orient = 0; mprotate(1,trans,true); selrotate(itemsel,trans,-1,0); }
		if(rot.y!=0){ trans.orient = 2; mprotate(1,trans,true); selrotate(itemsel,trans,-1,1); }
		if(rot.z!=0){ trans.orient = 4; mprotate(1,trans,true); selrotate(itemsel,trans,-1,2); }
	}
}

void itemflip(selinfo &itemsel,ivec &flip)
{
	loopi(3){
		if(flip[i]==-1){
			itemsel.orient = i*2;
			mpflip(itemsel,true);
		}
	}
}

void itemcopy(selinfo &sel)
{
	mpcopy(localedit,sel,true,linkcopys,linkcopygrid);
	// clear linkcopys cause of recursion
	linkcopys.shrink(0);
	linkcopygrid = 0;
}

void itemcopy(int *i)
{
	itemcopy(library[*i].sel);
}

void itemcopy(int *i,matrix3x3 &tm)
{
	// transform init
	selinfo curr_sel = library[*i].sel;
	ivec rot = ivec();
	ivec flip = ivec();
	decomptm(tm,rot,flip);
	
	//transform
	itemflip(curr_sel,flip);
	itemrotate(curr_sel,library[*i].trans,rot);
	
	//copy
	itemcopy(curr_sel);
	
	//transform back
	itemrotate(curr_sel,library[*i].trans,rot,true);
	itemflip(curr_sel,flip);
}

void itempaste(int *i,int *j)
{
	mppaste(localedit,library[*i].links[*j].sel,true,linkcopys,linkcopygrid);
}

//--- apply lib

void applyitemlink(int *i, int *j)
{
	itemcopy(i, library[*i].links[*j].tm);
	itempaste(i,j);
}
void applylibitem(int *i)
{
	if(*i<0 || *i+1>library.length()) return;
	loopitemilinks(*i){
		applyitemlink(i,&j);
	}
}

void applylib()
{
	looplibitems() applylibitem(&i);
}

COMMAND(applyitemlink,"ii");
COMMAND(applylibitem,"i");
COMMAND(applylib,"");

//--- repos, resize
/*
void tolibitem(int* i)
{
	if(*i<0 || *i+1>library.length()) return;
	library[*i] = iteminfo(sel);
	library[*i].clearlinks();
	conoutf("The selection is stored in the library under id %i.",*i);
}

COMMAND(tolibitem,"i");
*/


//--- select

void selectlibitem(int *i)
{
	sel = library[*i].sel;
	gridsize = library[*i].sel.grid;
	orient = library[*i].sel.orient;
	havesel = true;
	reorient();
}

COMMAND(selectlibitem,"i");

//----------------//
//-- Item Links --//
//----------------//

//--- add , delete(in selection)
void itemlink(int *i)
{
	if(*i<0 || *i+1>library.length()) return;
	itemcopy(i);
	mppaste(localedit,sel,true,linkcopys,linkcopygrid);
	library[*i].addlink(sel);
	reorient();
}

void itemlinkdelete(selinfo &sel)
{
	loopselitemlinksrev(sel) library[i].clearlink(j);
}

COMMAND(itemlink,"i");

//--- copy, paste

void itemlinkcopy(vector <linkinfo> &linkcopys, selinfo &sel, int &linkcopygrid)
{
	linkcopys.setsize(0); //real clear?? or better ~
	linkcopygrid = sel.grid;
	loopselitemlinks(sel) linkcopys.add(library[i].links[j]).sel.o.sub(sel.o);
}

void itemlinkpaste(vector <linkinfo> &linkcopys, selinfo &sel, int &linkcopygrid)
{
	float m = float(sel.grid)/float(linkcopygrid);
	loopv(linkcopys)
	{
		selinfo nsel = linkcopys[i].sel;
		vec o = nsel.o.tovec();
		o.mul(m).add(sel.o.tovec());
		nsel.o = ivec(o);
		nsel.grid *= m;
		if(nsel.grid>=1) linkcopys[i].parent->addlink(nsel,linkcopys[i].tm);
	}
}

//--- transform

//after rotate so u have sel as transformbox
void itemlinkrotate(int cw, selinfo &sel)
{
	int d = dimension(sel.orient);
	int scw = (!dimcoord(sel.orient))? -cw:cw;

	vec axis = vec(0,0,0);
	axis[d] = 1;
	matrix3x3 m = matrix3x3(vec(1,0,0),vec(0,1,0),vec(0,0,1));
	m.rotate(0,-cw,axis);
	loopselitemlinks(sel){
		library[i].links[j].tm.mul(library[i].links[j].tm,m);
		selrotate(library[i].links[j].sel,sel,scw,d);
	}
}

void itemlinkflip(selinfo &sel)
{
	int d = dimension(sel.orient);
	matrix3x3 m = matrix3x3(
		vec(((d==0)?-1:1),0,0),	
		vec(0,((d==1)?-1:1),0),
		vec(0,0,((d==2)?-1:1))
	);
	loopselitemlinks(sel){
		library[i].links[j].tm.mul(library[i].links[j].tm,m);
		library[i].links[j].sel.o.v[d] = sel.o.v[d] + sel.s.v[d] * sel.grid - library[i].links[j].sel.s.v[d] * library[i].links[j].sel.grid - library[i].links[j].sel.o.v[d] + sel.o.v[d];
	}
}

//--- select

void selectitemlink(int *i,int *j)
{
	sel = library[*i].links[*j].sel;
	gridsize = library[*i].links[*j].sel.grid;
	orient = library[*i].links[*j].sel.orient;
	havesel = true;
	reorient();
}

COMMAND(selectitemlink,"ii");

//------------//
//-- RENDER --//
//------------//

void rendertxtstart()
{
	defaultshader->set();   
	glEnable(GL_TEXTURE_2D);
}

void rendertxtstop()
{
	glDisable(GL_TEXTURE_2D);
	notextureshader->set();
}
void render_text(const char *str, vec& o, int r=255, int g=100, int b=0, int a=200)
{
	rendertxtstart();	
	
	float yaw = atan2f(o.y-camera1->o.y, o.x-camera1->o.x);
	glPushMatrix();
	glTranslatef(o.x, o.y, o.z);
	glRotatef(yaw/RAD-90, 0, 0, 1); 
	glRotatef(-90, 1, 0, 0);
	glScalef(-0.03, 0.03, 0.03);
	
	int tw, th;
	text_bounds(str, tw, th);
	draw_text(str, -tw/2, -th, r, g, b, 200);
	
	glPopMatrix();
	rendertxtstop();
}
void itembox3d(selinfo& sel,const char *str, int r=255, int g=100, int b=0, int a=200, float off=0.2f)
{
	vec size = sel.s.tovec().mul(sel.grid).add(2*off);
	vec o = sel.o.tovec().sub(off);
	glColor4ub(r, g, b, a);
	boxs3D(o, size, 1);
	glColor4ub(r, g, b,int(a/3));
	boxs3D(o, vec(1).mul(sel.grid).add(2*off), 1);
	o.add( vec(size.x/2, size.y/2, size.z));
	render_text(str, o, r ,g ,b ,a);
}
void renderlib()
{
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	looplibitems()
	{
		string ititle;
		getitemname(i,ititle);
		itembox3d(library[i].sel, ititle);
		
		itembox3d(library[i].trans, "", 255, 0, 0,100,.3f);
		
		loopitemilinks(i)
		{
			// debug output
			
			ivec rot = ivec();
			ivec flip = ivec();
			decomptm(library[i].links[j].tm,rot,flip);
			defformatstring(linktitle)(
				"itemlink %i:%i | %.0f %.0f %.0f ; %.0f %.0f %.0f ; %.0f %.0f %.0f | rot(x:%i,y:%i,z:%i) flip(x:%i,y:%i,z:%i)"
				,i,j,
				library[i].links[j].tm.a.x,library[i].links[j].tm.a.y,library[i].links[j].tm.a.z,
				library[i].links[j].tm.b.x,library[i].links[j].tm.b.y,library[i].links[j].tm.b.z,
				library[i].links[j].tm.c.x,library[i].links[j].tm.c.y,library[i].links[j].tm.c.z,
				rot[0],rot[1],rot[2],
				(flip[0]==-1)?1:0,(flip[1]==-1)?1:0,(flip[2]==-1)?1:0
			);
			
			//defformatstring(linktitle)("%s:%i",ititle,j);

			itembox3d(library[i].links[j].sel, linktitle, 0, 255, 100);
		}
	}
	
	glDisable(GL_BLEND);
}

//--------//
//-- IO --//
//--------//

void save_sel(stream *f,selinfo &sel){
	defformatstring(str)("%d %d %d  %d %d %d  %d %d",
		sel.s.x,sel.s.y,sel.s.z,
		sel.o.x,sel.o.y,sel.o.z,
		sel.grid,sel.orient
	);
	f->putline(str);
}
void save_tm(stream *f,matrix3x3 &tm){
	defformatstring(str)("%.0f %.0f %.0f  %.0f %.0f %.0f  %.0f %.0f %.0f",
		tm.a.x,tm.a.y,tm.a.z,
		tm.b.x,tm.b.y,tm.b.z,
		tm.c.x,tm.c.y,tm.c.z
	);
	f->putline(str);
}

bool save_library(string &libname){
	stream *f = opengzfile(libname, "wb");
	if(!f) { conoutf(CON_WARN, "could not write lib to %s", libname); return false; }
	f->putlil<int>(library.length());
	f->putchar('\n');
	looplibitems(){
		save_sel(f,library[i].sel);
		conoutf("put %i",f->putline(library[i].name));
		f->putlil<int>(library[i].links.length());
		f->putchar('\n');
		loopitemilinks(i){
			save_sel(f,library[i].links[j].sel);
			save_tm(f,library[i].links[j].tm);
		}
	}
	delete f;
	conoutf("wrote map file %s", libname);
	return true;
}

selinfo load_sel(stream *f){
	selinfo s = selinfo();
	char buf[512];
	f->getline(buf, sizeof(buf));
	sscanf(buf,"%d %d %d  %d %d %d  %d %d",
		&s.s.x,&s.s.y,&s.s.z,
		&s.o.x,&s.o.y,&s.o.z,
		&s.grid,&s.orient
	);
	return s;
}
matrix3x3 load_tm(stream *f){
	matrix3x3 m = matrix3x3();
	char buf[512];
	f->getline(buf, sizeof(buf));
	sscanf(buf,"%f %f %f  %f %f %f  %f %f %f",
		&m.a.x,&m.a.y,&m.a.z,
		&m.b.x,&m.b.y,&m.b.z,
		&m.c.x,&m.c.y,&m.c.z
	);
	return m;
}

bool load_library(string &libname){
	library.setsize(0);
	stream *f = opengzfile(libname, "rb");
	if(!f) { conoutf(CON_ERROR, "could not read lib %s", libname); return false; }
	int ilen = f->getlil<int>();
	f->getchar();
	loopi(ilen){
		library.add(iteminfo(load_sel(f)));

		char buf[512];
		f->getline(buf,sizeof(buf));
		int blen = strlen(buf);
		if(!(blen > 0 && buf[blen-1] == '\n')) newstring(buf);

		int llen = f->getlil<int>();
		f->getchar();
		loopj(llen){
			library[i].addlink(load_sel(f));
			library[i].links[j].tm = load_tm(f);
		}
	}
	delete f;
	conoutf("read lib %s", libname);
	return true;
}

//----------//
//-- Menu --//
//----------//

void getlibitemslist()
{
	vector<char> buf;
	string item;
	looplibitems()
	{
		formatstring(item)("\"libitem %i\"", i);
        buf.put(item, strlen(item));
	}
	buf.add('\0');
	result(buf.getbuf());
}

ICOMMAND(libitems, "", (), getlibitemslist());
ICOMMAND(libitemscnt, "", (), intret(library.length()));
ICOMMAND(linkscnt, "i", (int *i), intret(library[*i].links.length()));
