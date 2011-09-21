#include "engine.h"

extern selinfo sel;
extern void changed(const block3 &sel, bool commit = true);

//selection from the scanned circuit
selinfo circsel;
bool scaned = false;

vector <IO_elem*> elements;
vector <IO_elem*> I_elements;
vector <IO_elem*> IO_renderables;
vector <IO_elem*> IO_colidables;

#define loopelements() loopv(elements)
#define loopielements() loopv(I_elements)
#define looprelements() loopv(IO_renderables)
#define loopcelements() loopv(IO_colidables)

// for simulation
unsigned int simulating_step = 0;

// faces
int xi[6] = {-1, 1, 0, 0, 0, 0};  
int yi[6] = { 0, 0,-1, 1, 0, 0};
int zi[6] = { 0, 0, 0, 0,-1, 1};

// oposit face
int of[6] = {1, 0, 3, 2, 5, 4}; 

//TODO all circ textures at last so is texture needs only to ask if tex > ...
bool is_io_tex(cube *c)
{
	switch(c->texture[O_BOTTOM])
	{
		case TEX_WIRE_ON:
		case TEX_WIRE_ONE_WAY:
		case TEX_SWITCH_ON:
		case TEX_SWITCH_OFF:
		case TEX_SWITCH_COLLIDE_ON:
		case TEX_SWITCH_COLLIDE_OFF:
		case TEX_TORCH_ON:
		case TEX_TORCH_OFF:
		case TEX_LED_ON:
		case TEX_LED_OFF:
		case TEX_BUTTON_ON:
		case TEX_BUTTON_OFF:
		case TEX_RADIO_BUTTON_OFF:
		case TEX_RADIO_BUTTON_ON:
		case TEX_BUFFER:
		case TEX_RECTANGLE_SIGNAL_SWITCH:

		case TEX_RECTANGLE_SIGNAL:
		case TEX_TIMER:
		case TEX_TIME_LOW:

		case TEX_SPEAKER:
		case TEX_PUNCHCARD:
		case TEX_INOUT_BIT:
		case TEX_OUT_BIT_OFF:
		case TEX_OUT_BIT_ON:
			return true;
	}
	return false;
}

cube &lookupcube_no_divide(int tx, int ty, int tz, int tsize, bool &found)
{
    tx = clamp(tx, 0, worldsize-1);
    ty = clamp(ty, 0, worldsize-1);
    tz = clamp(tz, 0, worldsize-1);
    int scale = worldscale-1, csize = abs(tsize);
    cube *c = &worldroot[octastep(tx, ty, tz, scale)];
	found = true;
    if(!(csize>>scale)) do
    {
		if(!c->children)
		{
			if(is_io_tex(c) && tsize > 0){
				subdividecube(*c);
				scale--;
				c = &c->children[octastep(tx, ty, tz, scale)];
			}else{
				found = false;
			}
		}else{
			scale--;
			c = &c->children[octastep(tx, ty, tz, scale)];
		}
    } while(!(csize>>scale) && found);

    return *c;
}


// sel loops
#define loopsel(f){ loopi(sel.s.x) loopj(sel.s.y) loopk(sel.s.z) { int x=sel.o.x+i*sel.grid; int y=sel.o.y+j*sel.grid; int z=sel.o.z+k*sel.grid; bool found = false; cube *c = &lookupcube_no_divide(x,y,z,sel.grid,found); if(found) f; }}
#define loopcircsel(f){ loopi(circsel.s.x) loopj(circsel.s.y) loopk(circsel.s.z) { int x=circsel.o.x+i*circsel.grid; int y=circsel.o.y+j*circsel.grid; int z=circsel.o.z+k*circsel.grid; bool found = false; cube *c = &lookupcube_no_divide(x,y,z,circsel.grid,found); if(found) f; }}

void settimes_elem(int htime, int ltime,IO_elem *e){
	switch(e->etype){
		case IOE_RECTANGLE_SIGNAL:
			((RECTANGLE_SIGNAL*)e)->set_time(htime,ltime);
			break;
		case IOE_BUTTON:
			((BUTTON*)e)->set_time(htime);
			break;
		case IOE_RECTANGLE_SIGNAL_SWITCH:
			((RECTANGLE_SIGNAL_SWITCH*)e)->set_time(htime,ltime);
			break;
		case IOE_SPEAKER:
			((SPEAKER*)e)->set_frequenz((double)htime + (double)ltime/1000);
			break;
		default:
			conoutf("\f1No timing element!\f1");
			break;
	}
}

void gettimes_elem(IO_elem *e){
	switch(e->etype){
		case IOE_RECTANGLE_SIGNAL:
			conoutf("high time: %d ms; low time: %d ms",((RECTANGLE_SIGNAL*)e)->high_time,((RECTANGLE_SIGNAL*)e)->low_time);
			break;
		case IOE_BUTTON:
			conoutf("high time: %d ms",((BUTTON*)e)->high_time);
			break;
		case IOE_RECTANGLE_SIGNAL_SWITCH:
			conoutf("high time: %d ms; low time: %d ms",((RECTANGLE_SIGNAL_SWITCH*)e)->high_time, ((RECTANGLE_SIGNAL_SWITCH*)e)->low_time);
			break;
		case IOE_SPEAKER:
			conoutf("frequenz: %.3f Hz",((SPEAKER*)e)->frequenz);
			break;
		default:
			conoutf("\f1No timing element!\f1");
			break;
	}
}


/* Commands */

void lreset(){
	loopelements() elements[i]->reset();
	simulating_step = 0;
}
COMMAND(lreset,""); // reset states of elements

void lclearall(bool change)
{
	if(!scaned) return;
	scaned = false;
	loopcircsel( c->lwire_scanned = 0; c->io_elem = NULL; );
	loopelements(){
		//stop all sounds
		if(elements[i]->etype == IOE_SPEAKER) ((SPEAKER *)elements[i])->stop();
	}
	cube *c;
	looprelements()
	{ 
		c = &lookupcube(IO_renderables[i]->x, IO_renderables[i]->y, IO_renderables[i]->z, circsel.grid);
		loopj(3) c->faces[j] = IO_renderables[i]->faces[j];
	}
	
	elements.deletecontents();
	I_elements.setsize(0);
	IO_renderables.setsize(0);
	IO_colidables.setsize(0);
	if(change) changed(circsel);
}
void lclear()
{
	lclearall(true);
}
COMMAND(lclear,""); // clear circuit

bool is_tex_cube(cube *c,const int tex)
{
	if(!isempty(*c)) if(c->texture[O_BOTTOM] == tex) return true;
	return false;
}

bool is_cube_etype(cube *c, uint etype)
{
	if(!c->io_elem) return false;
	return (c->io_elem->etype == etype) ? true : false;
}

void init_element_times(IO_elem *e,bool inoutbitable = false)
{
	int htime = 0;
	int ltime = 0;

	cube *c;
	int x,y,z;
	int nx,ny,ztop,zbot;
	int id;
	INOUT_BIT * iob;
	bool found = false;

	loopi(4)
	{
		// only one direction on xy plane
		x = e->x + xi[i]*circsel.grid;
		y = e->y + yi[i]*circsel.grid;
		z = e->z;

		//c = &lookupcube(x,y,z, circsel.grid);
		c = &lookupcube_no_divide(x,y,z, circsel.grid,found);
		if(found)
		{
			if(is_tex_cube(c,TEX_TIMER))
			{
				// Timer input found
				ztop = z+circsel.grid;
				zbot = z-circsel.grid;

				loopk(MAX_TIMER_FREQUENCY)
				{
					nx = x + k*xi[i]*circsel.grid;
					ny = y + k*yi[i]*circsel.grid;
					//c = &lookupcube(nx,ny,z, circsel.grid);
					c = &lookupcube_no_divide(nx,ny,z, circsel.grid,found);
					if(found)
					{
						if(is_tex_cube(c,TEX_TIMER))
						{
							// look if has a high and a low
							//c = &lookupcube(nx,ny,z+circsel.grid, circsel.grid);
							c = &lookupcube_no_divide(nx,ny,ztop, circsel.grid,found);
							if(found)
							{
								if(is_tex_cube(c,TEX_TIME_HIGH)) htime += 1<<k;
								if(inoutbitable)
								{
									if(is_cube_etype(c, IOE_INOUT_BIT))
									{
										iob = ((INOUT_BIT *) c->io_elem);
										((IO_elem_timer_inoutbitable *) e)->add_hinout_bit(iob);
										iob->index = k;
									}
								}
							}

							c = &lookupcube_no_divide(nx,ny,zbot, circsel.grid,found);
							if(found)
							{
							//c = &lookupcube(nx,ny,z-circsel.grid, circsel.grid);
								if(is_tex_cube(c,TEX_TIME_LOW)) ltime += 1<<k;
								if(inoutbitable)
								{
									if(is_cube_etype(c, IOE_INOUT_BIT))
									{
										iob = ((INOUT_BIT *) c->io_elem);
										((IO_elem_timer_inoutbitable *) e)->add_linout_bit(iob);
										iob->index = k;
									}
								}
							}
						}else{
							//break;
							k = MAX_TIMER_FREQUENCY;
						}
					}else{
						k = MAX_TIMER_FREQUENCY;
					}
				}
			}
		}
	}
	if(htime!=0 || ltime!=0) settimes_elem(htime, ltime,e);
}

void init_punchcard(IO_elem *e)
{
	cube *c;
	int x,y,z;
	int nx,ny,ztop,zbot;
	int dir = -1;
	bool found_card = false;
	bool found = false;
	loopi(4)
	{
		// only one direction on xy plane
		x = e->x + xi[i]*circsel.grid;
		y = e->y + yi[i]*circsel.grid;
		z = e->z;
		c = &lookupcube_no_divide(x,y,z, circsel.grid,found);
		if(found)
		{
			if(is_tex_cube(c,TEX_PUNCHCARD))
			{
				//punchcard found get width
				PUNCHCARD *pc = ((PUNCHCARD *)elements.add(new PUNCHCARD()));
				pc->outputs.add(e);
				found_card = true;
			
				//look for punchcard inputs
				loopj(4)
				{
					if(j==of[i]) continue; //not the signal
					// only one direction on xy plane
					nx = x + xi[j]*circsel.grid;
					ny = y + yi[j]*circsel.grid;
					c = &lookupcube_no_divide(nx,ny,z, circsel.grid,found);
					if(found)
					{
						if(is_cube_etype(c, IOE_OUT_BIT)){
							dir = of[j];
							//found input
							while(c->io_elem && found){
								found = false;
								if(is_cube_etype(c, IOE_OUT_BIT)){
									pc->inputs.add(c->io_elem);
									c->io_elem->outputs.add(pc);
									((OUT_BIT *)c->io_elem)->index = pc->width;
									pc->width++;
									nx += xi[i]*circsel.grid;
									ny += yi[i]*circsel.grid;
									c = &lookupcube_no_divide(nx,ny,z, circsel.grid,found);
								}
							}
							j=4; //break
						}
					}
				}
				
				//read punchcard
				nx = x;
				ny = y;
				ztop = z + circsel.grid;
				zbot = z - circsel.grid;
			
				if(dir>=0 && pc->width>0)
				{
					c = &lookupcube_no_divide(nx, ny, z, circsel.grid,found);
					while(is_tex_cube(c,TEX_PUNCHCARD) && found)
					{
						//read high bits
						for(int cw=0;cw<pc->width;cw++)
						{
							//read data
							c = &lookupcube_no_divide(nx+cw*xi[i]*circsel.grid, ny+cw*yi[i]*circsel.grid, ztop, circsel.grid,found);
							if(is_tex_cube(c,TEX_TIME_HIGH) && found) pc->card.add(true);
							else pc->card.add(false);
						
							//read outbit row
							c = &lookupcube_no_divide(nx+cw*xi[i]*circsel.grid, ny+cw*yi[i]*circsel.grid, zbot, circsel.grid,found);
							if(found)
							{
								if(is_cube_etype(c, IOE_OUT_BIT))
								{
									pc->row.add(((OUT_BIT *)c->io_elem));
									c->io_elem->outputs.add(pc);
									((OUT_BIT *)c->io_elem)->index = pc->length;
								}
							}
						}

						pc->length++;

						nx += xi[dir]*circsel.grid;
						ny += yi[dir]*circsel.grid;
						c = &lookupcube_no_divide(nx,ny,z, circsel.grid,found);
					}

					// look for back btn
					nx -= xi[dir]*circsel.grid;
					ny -= yi[dir]*circsel.grid;
					c = &lookupcube_no_divide(nx+(pc->width)*xi[i]*circsel.grid, ny+(pc->width)*yi[i]*circsel.grid, z, circsel.grid,found);
					if(c->io_elem) if(c->io_elem->etype==IOE_INOUT_BIT) pc->outputs.add(c->io_elem);
					i = 4; //break
				}
				else if(found_card)
				{
					//not set up correctly or back btn
					elements.pop();
					delete pc;
				}
			}
		}
	}
}

void add_renderable(IO_elem *e, cube *c)
{
	IO_renderables.add(e);
	
	loopj(3){
		e->faces[j] = c->faces[j];
		c->faces[j] = 0x71717171;
	}
}

struct wiredata
{
	int x,y,z;
	wiredata(int _x,int _y,int _z){
		x=_x;
		y=_y;
		z=_z;
	}
};
vector <wiredata*> current_wire_parts;

void wire_step(int ei,int wi,IO_elem *e,int faces=6){
	cube *nc;
	int nx,ny,nz;
	int scanstep = ei+1;
	bool found = false;
	loopi(faces){
		found = false;
		nx = current_wire_parts[wi]->x + xi[i]*circsel.grid;
		ny = current_wire_parts[wi]->y + yi[i]*circsel.grid;
		nz = current_wire_parts[wi]->z + zi[i]*circsel.grid;
		nc = &lookupcube_no_divide(nx,ny,nz, circsel.grid,found);
		if(found){
			if (!isempty(*nc) && nc->lwire_scanned!=scanstep)
			{
			
				if(nc->io_elem)
				{
					if((nc->io_elem->type == IO_TYPE_IO && i==O_BOTTOM) || (nc->io_elem->type == IO_TYPE_O))
					{
						e->outputs.add(nc->io_elem);
						nc->lwire_scanned = scanstep;
					}
				}
				else if((nc->texture[O_BOTTOM] == TEX_WIRE_ON || nc->texture[O_BOTTOM] == TEX_WIRE_ONE_WAY) && nc->texture[of[i]] != TEX_WIRE_ONE_WAY)
				{	
					current_wire_parts.add(new wiredata(nx,ny,nz)); // add to wire parts to be checked in next step
					nc->lwire_scanned = scanstep;
				}
			}
		}
	}
	delete current_wire_parts[wi];
	current_wire_parts.remove(wi);
}

int logic_progress = 0;
int logic_progress_total = 0;
void lscan() {
	int smills = SDL_GetTicks(); // time delay for debug

	lclearall(true);

	circsel = sel;
	// collect elements
	
	loopsel(
		if(!isempty(*c))
		{
			switch(c->texture[O_BOTTOM])
			{
				case TEX_SWITCH_ON:
					c->io_elem = new SWITCH(x ,y ,z, true);
					elements.add(c->io_elem);
					add_renderable(c->io_elem, c);
					break;
				case TEX_SWITCH_OFF:
					c->io_elem = new SWITCH(x ,y ,z, false);
					elements.add(c->io_elem);
					add_renderable(c->io_elem, c);
					break;
				case TEX_SWITCH_COLLIDE_OFF:
					c->io_elem = new SWITCH_COLLIDE(x ,y ,z, false);
					IO_colidables.add(elements.add(c->io_elem));
					add_renderable(c->io_elem, c);
					break;
				case TEX_TORCH_ON:
					c->io_elem = new TORCH(x ,y ,z, true);
					elements.add(c->io_elem);
					break;	
				case TEX_TORCH_OFF:
					c->io_elem = new TORCH(x ,y ,z, false);
					elements.add(c->io_elem);
					break;
				case TEX_LED_ON:
					c->io_elem = new LED(x ,y ,z, true); //I elements to I_elements for simulation (only I not IO)
					I_elements.add(elements.add(c->io_elem));
					add_renderable(c->io_elem, c);
					break;
				case TEX_LED_OFF:
					c->io_elem = new LED(x ,y ,z, false); //I elements to I_elements for simulation (only I not IO)
					I_elements.add(elements.add(c->io_elem));
					add_renderable(c->io_elem, c);
					break;
				case TEX_RECTANGLE_SIGNAL:
					c->io_elem = new RECTANGLE_SIGNAL(x ,y ,z, false);
					elements.add(c->io_elem);
					break;
				case TEX_RECTANGLE_SIGNAL_SWITCH:
					c->io_elem = new RECTANGLE_SIGNAL_SWITCH(x ,y ,z, false);
					elements.add(c->io_elem);
					break;
				case TEX_BUTTON_ON:
					c->io_elem = new BUTTON(x ,y ,z, true);
					elements.add(c->io_elem);
					add_renderable(c->io_elem, c);
					break;
				case TEX_BUTTON_OFF:
					c->io_elem = new BUTTON(x ,y ,z, false);
					elements.add(c->io_elem);
					add_renderable(c->io_elem, c);
					break;
				case TEX_RADIO_BUTTON_ON:
					c->io_elem = new RADIO_BUTTON(x ,y ,z, true);
					elements.add(c->io_elem);
					add_renderable(c->io_elem, c);
					break;
				case TEX_RADIO_BUTTON_OFF:
					c->io_elem = new RADIO_BUTTON(x ,y ,z, false);
					elements.add(c->io_elem);
					add_renderable(c->io_elem, c);
					break;
				case TEX_BUFFER:
					c->io_elem = new BUFFER(x ,y ,z, false);
					elements.add(c->io_elem);
					break;
				case TEX_SPEAKER:
					c->io_elem = new SPEAKER(x ,y ,z, false); //I elements to I_elements for simulation (only I not IO)
					I_elements.add(elements.add(c->io_elem));
					break;
				case TEX_INOUT_BIT:
					c->io_elem = new INOUT_BIT(x ,y ,z, false);
					elements.add(c->io_elem);
					break;
				case TEX_OUT_BIT_OFF:
					c->io_elem = new OUT_BIT(x ,y ,z, false);
					elements.add(c->io_elem);
					break;
				case TEX_OUT_BIT_ON:
					c->io_elem = new OUT_BIT(x ,y ,z, true);
					elements.add(c->io_elem);
					break;
			}
		}
	);
	

	// scann connections
	logic_progress = 0;
	logic_progress_total = elements.length();
	
	cube *nc;
	IO_elem *e;
	loopelements()
	{
		e = elements[i];
		// only input elements connections are relevant
		if(e->type==IO_TYPE_IO || e->type==IO_TYPE_I){
			// add current element to wire done and wire parts
			current_wire_parts.add(new wiredata(e->x,e->y,e->z));
			nc = &lookupcube(e->x,e->y,e->z,circsel.grid);
			nc->lwire_scanned = i+1;

			// check sourounding elements for connected output or wires
			if(e->type==IO_TYPE_IO) wire_step(i, 0, e, 5);//5 faces cause on top is output
			if(e->type==IO_TYPE_I) wire_step(i, 0, e, 6); //6 faces all faces can be input
			
			// while wires to walk check all wires sourounding for connected output or wires
			while(!current_wire_parts.empty())
			{
				for(int wi=current_wire_parts.length()-1;wi>=0;wi--){
					wire_step(i,wi,e);
				}
			}
		}
		logic_progress++;
		renderprogress(float(logic_progress)/logic_progress_total, "scanning connections...");
	}
	
	//init timers
	loopelements()
	{
		switch(elements[i]->etype){
			case IOE_BUTTON:
				init_element_times(elements[i]);
				break;
			case IOE_SPEAKER:
			case IOE_RECTANGLE_SIGNAL:
			case IOE_RECTANGLE_SIGNAL_SWITCH:
				init_element_times(elements[i],true);
				break;
		}
	}
	
	//init punchcard
	loopelements() if(elements[i]->etype == IOE_INOUT_BIT) init_punchcard(elements[i]);

	changed(circsel);
	conoutf("\f1%d Elements found. In %f sec\f1",elements.length(),(float)(SDL_GetTicks() - smills)/1000.0f);
	scaned = true;
}
COMMAND(lscan,""); // scans selection and adds new elements to circuit

void mplscan(selinfo &_sel, bool local)
{
	if(local) game::edittrigger(_sel, EDIT_LSCAN);
	sel = _sel;
	lscan();
}

void mplscan()
{
	mplscan(sel,true);
}

COMMAND(mplscan,""); // scans selection and adds new elements to circuit

void lrescan() {
	sel = circsel;
	lscan();
}
COMMAND(lrescan,"");
/*
void lstep(){
	int smills = SDL_GetTicks(); // time delay for debug

	simulating_step++; // later timer simulation like physic engine avoid overflow
	conoutf("step %d",simulating_step);
	loopielements()
	{
		I_elements[i]->sim(simulating_step, 1);
	}
	
	 // time delay output for debug
	int duration = SDL_GetTicks() - smills;
	conoutf("simulation duration was %d ms (%f s)",duration,(float)duration/1000.0f);
	conoutf("simulation per element was %f ms (%f s)",(float)duration/(float)elements.length(),(float)duration/(float)elements.length()/1000.0f);
}
COMMAND(lstep,"");
*/

#define circuitgriderr if(sel.grid!=circsel.grid) { conoutf(CON_ERROR, "\f3gridpower should be %d\f3",(int)sqrt((float)circsel.grid)); return; }

void lset(){
	circuitgriderr;
	bool found;
	cube *c = &lookupcube_no_divide(sel.o.x,sel.o.y,sel.o.z,sel.grid,found);
	if(found && c->io_elem){
		c->io_elem->toggle();
	}else{
		conoutf("\f1No circuit element!\f1");
	}
}
COMMAND(lset,""); //toggle a element state

void lsettimes(int *htime, int *ltime)
{
	circuitgriderr;
	bool found;
	cube *c = &lookupcube_no_divide(sel.o.x,sel.o.y,sel.o.z,sel.grid,found);
	if(found && c->io_elem){
		settimes_elem(*htime, *ltime,c->io_elem);
	}else{
		conoutf("\f1No circuit element!\f1");
	}
}
COMMAND(lsettimes,"ii");

void lgettimes()
{
	circuitgriderr;
	//IO_elem *e = findelement(sel.o.x,sel.o.y,sel.o.z);
	bool found;
	cube *c = &lookupcube_no_divide(sel.o.x,sel.o.y,sel.o.z,sel.grid,found);
	if(found && c->io_elem){
		gettimes_elem(c->io_elem);
	}else{
		conoutf("\f1No circuit element!\f1");
	}
}
COMMAND(lgettimes,"");

VAR(lpause, 0, 0, 1);

extern void editface(int *dir, int *mode);
extern void edittex(int i, bool save = true);
extern void delcube();
extern int allfaces;

void buildcube(const int tex){
	sel.o.x -= sel.grid;
	int dir = -1;
	int mode = 1;
	editface(&dir, &mode);
	edittex(tex,false);
}

void lbuildtimer(int *htime, int *ltime)
{
	delcube();
	sel.s.x = 1;
	sel.s.y = 1;
	sel.s.z = 1;
	//sel.grid = 4;
	sel.orient = 1;
	sel.cx=0;
	sel.cy=0;
	sel.cxs=2;
	sel.cys=2;
	sel.corner=3;

	int allfs = allfaces;
	allfaces = 1;
	buildcube(TEX_RECTANGLE_SIGNAL);

	int ht = *htime;
	int lt = *ltime;

	int maxtime = max(ht,lt);
	if(maxtime==0) return;
	int cnt = 0;
	while(maxtime!=0){
		sel.o.x += sel.grid;
		buildcube(TEX_TIMER);
		
		if(ht&1<<cnt){
			sel.o.z += sel.grid;
			buildcube(TEX_TIME_HIGH);
			sel.o.z -= sel.grid;
		}
		if(lt&1<<cnt){
			sel.o.z -= sel.grid;
			buildcube(TEX_TIME_LOW);
			sel.o.z += sel.grid;
		}
		
		maxtime>>=1;
		cnt++;
	}
	allfaces = allfs;
	cancelsel();
}
COMMAND(lbuildtimer,"ii"); // build a timer input in ms

/* Rendering */

void draw_ioelem_face(float s0, float t0, int x0, int y0, int z0,
                      float s1, float t1, int x1, int y1, int z1,
                      float s2, float t2, int x2, int y2, int z2,
                      float s3, float t3, int x3, int y3, int z3)
{
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(s0, t0); glVertex3f(x0, y0, z0);
    glTexCoord2f(s1, t1); glVertex3f(x1, y1, z1);
	glTexCoord2f(s3, t3); glVertex3f(x3, y3, z3);
    glTexCoord2f(s2, t2); glVertex3f(x2, y2, z2);
    glEnd();
    xtraverts += 4;
}

void render_cube(int x, int y, int z,int size, float tc[4][2], int faces=0x3F)
{
	int sx = x+size;
	int sy = y+size;
	int sz = z+size;
	
	if(faces&0x01)
        draw_ioelem_face(tc[0][0], tc[0][1],  sx, sy, sz,
                         tc[1][0], tc[1][1],  sx,  y, sz,
                         tc[2][0], tc[2][1],  sx,  y, z,
                         tc[3][0], tc[3][1],  sx, sy, z);

    if(faces&0x02)
        draw_ioelem_face(tc[2][0], tc[2][1], x, sy, z,
                         tc[3][0], tc[3][1], x,  y, z,
                         tc[0][0], tc[0][1], x,  y, sz,
                         tc[1][0], tc[1][1], x, sy, sz);

    if(faces&0x04)
        draw_ioelem_face(tc[2][0], tc[2][1], sx, sy, z,
                         tc[3][0], tc[3][1],  x, sy, z,
                         tc[0][0], tc[0][1],  x, sy, sz,
                         tc[1][0], tc[1][1], sx, sy, sz);

    if(faces&0x08)
        draw_ioelem_face(tc[2][0], tc[2][1],  x,  y, z,
                         tc[3][0], tc[3][1], sx,  y, z,
                         tc[0][0], tc[0][1], sx,  y, sz,
                         tc[1][0], tc[1][1],  x,  y, sz);

    if(faces&0x10)
        draw_ioelem_face(tc[3][0], tc[3][1], sx,  y,  z,
                         tc[0][0], tc[0][1],  x,  y,  z,
                         tc[1][0], tc[1][1],  x, sy,  z,
                         tc[2][0], tc[2][1], sx, sy,  z);

    if(faces&0x20)
        draw_ioelem_face(tc[3][0], tc[3][1],  x,  y, sz,
                         tc[0][0], tc[0][1], sx,  y, sz,
                         tc[1][0], tc[1][1], sx, sy, sz,
                         tc[2][0], tc[2][1],  x, sy, sz);
}

//TODO: use shaders 
void render_with_txture(uint _i,int _x, int _y, int _z)
{
    VSlot &vslot = lookupvslot(_i);
    Slot &slot = *vslot.slot;
    Texture *tex = slot.sts.empty() ? notexture : slot.sts[0].t;
	Texture *glowtex = NULL;
	
    loopvj(slot.sts){
		if(slot.sts[j].type==TEX_GLOW) glowtex = slot.sts[j].t;
	}

	//scale to gridsize 
	//TODO: scale offset rot from vslot
	float sg = circsel.grid * 8; // TODO: its maybe wrong
	float sx = sg/(float)tex->xs;
	float sy = sg/(float)tex->ys;
	float tc[4][2] = { { 0, 0 }, { sx, 0 }, { sx, sy }, { 0, sy } };
	
	//glActiveTexture_(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tex->id);
	render_cube(_x,_y,_z,circsel.grid,tc);

	//g
	if(glowtex)
	{
		//glActiveTexture_(GL_TEXTURE1);
		glDepthFunc(GL_EQUAL);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glBindTexture(GL_TEXTURE_2D, glowtex->id);

		render_cube(_x,_y,_z,circsel.grid,tc);

		glDisable(GL_BLEND);
		glDepthFunc(GL_LESS);
	}
}

void renderlogicsim(){
	if(!scaned) return;
	glEnable(GL_TEXTURE_2D);
	looprelements() render_with_txture(IO_renderables[i]->render_result() ,IO_renderables[i]->x, IO_renderables[i]->y, IO_renderables[i]->z);
	glDisable(GL_TEXTURE_2D);
}

/* Physic */

/* collidable elements */
bool cube_player_aabb_colli(ivec co,int size, physent *d)
{
	int crad = size/2;
	int off = 1; // 1 offset
	if(
		fabs(d->o.x - co.x - crad) >= d->radius + off + crad || 
		fabs(d->o.y - co.y - crad) >= d->radius + off + crad || 
		d->o.z + d->aboveeye + off <= co.z || 
		d->o.z - d->eyeheight - off >= co.z + size) return false;
	return true;
};


void lcollidebutton(){
	bool coll;
	loopcelements(){
		coll = false;
		loopvj(game::players)
		{
			physent *d = game::players[j];
			if(cube_player_aabb_colli(ivec(IO_colidables[i]->x, IO_colidables[i]->y, IO_colidables[i]->z), circsel.grid, d)) coll = true;
		}
		
		IO_colidables[i]->sim_state = coll;
	}
}

/* logic sim */
VARP(ltimestep, 1, 1, 3000);
int curr_time = 0;

void updatelogicsim(int curtime)
{
	if(!scaned || curtime==0 || lpause) return;

	// ms loop
	lcollidebutton();
	loopi(curtime)
	{
		//step time in ms
		curr_time++;
		if(curr_time>=ltimestep){
			curr_time=0;
			simulating_step++;
			if(simulating_step>1000) simulating_step = 0;
			loopielements() I_elements[i]->sim(simulating_step,1); //only inputs to sim
		}
	}
}

/* shoot toggle */
bool incube(const vec &v, const vec &bo, int size)
{
    return v.x <= bo.x+size &&
           v.x >= bo.x &&
           v.y <= bo.y+size &&
           v.y >= bo.y &&
           v.z <= bo.z+size &&
           v.z >= bo.z;
}

void lshoottoogle(const vec &to)
{
	loopelements()
	{
		if(elements[i]->etype == IOE_SWITCH || elements[i]->etype == IOE_BUTTON || elements[i]->etype == IOE_RADIO_BUTTON)
		{
			if(incube(to, vec(elements[i]->x,elements[i]->y,elements[i]->z), circsel.grid)) elements[i]->toggle();
		}
	}
}


//debug
/*
void xyz(){
	conoutf("x=%d y=%d z=%d",sel.o.x,sel.o.y,sel.o.z);
}
COMMAND(xyz,"");


void changedsel()
{
	changed(sel);
}
COMMAND(changedsel,"");

void haschilds()
{
	cube *c = &lookupcube(sel.o.x,sel.o.y,sel.o.z,sel.grid);
	conoutf("no childs %i",(!c->children));
}
COMMAND(haschilds,"");

void cubeempty()
{
	cube *c = &lookupcube(sel.o.x,sel.o.y,sel.o.z,sel.grid);
	conoutf("childs %i",isempty(*c));
}
COMMAND(cubeempty,"");
*/