//selection from the scanned circuit
selinfo circsel;

// texture index for scanning and rendering io elements
const uint TEX_WIRE_ON = 834;
const uint TEX_WIRE_ONE_WAY = 832;
const uint TEX_SWITCH_ON = 456;
const uint TEX_SWITCH_OFF = 417;
const uint TEX_TORCH_ON = 774;
const uint TEX_TORCH_OFF = 776;
const uint TEX_LED_ON = 764;
const uint TEX_LED_OFF = 814;
const uint TEX_BUTTON_ON = 464;
const uint TEX_BUTTON_OFF = 418;
const uint TEX_RADIO_BUTTON_OFF = 113;
const uint TEX_RADIO_BUTTON_ON = 201;
const uint TEX_BUFFER = 780;

const uint TEX_RECTANGLE_SIGNAL = 833;
const uint TEX_TIMER = 784;
const uint TEX_TIME_HIGH = 832;
const uint TEX_TIME_LOW = 831;

// directions for all 6 neighbors
int xi[6] = {-1, 1, 0, 0, 0, 0};  
int yi[6] = { 0, 0,-1, 1, 0, 0};
int zi[6] = { 0, 0, 0, 0,-1, 1};

// oposit face
int of[6] = {1, 0, 3, 2, 5, 4}; 

// possible IO types (Output,InOutput,Input)
enum IO_types
{
	IO_TYPE_O,
	IO_TYPE_IO,
	IO_TYPE_I
};

// all io element types
enum IO_elem_type
{
	IOE_UNDEFINED,
	IOE_SWITCH,
	IOE_TORCH,
	IOE_LED,
	IOE_RECTANGLE_SIGNAL,
	IOE_BUTTON,
	IOE_RADIO_BUTTON,
};

// for simulation
unsigned int simulating_step = 0;

/* IO Elements */

struct IO_elem
{
	cube* c;
	int x,y,z;
	unsigned int type; // see IO_types
	unsigned int etype; // see IO_elem_type
	bool default_state;
	bool sim_state; // high or low
	unsigned int sim_step; // to checks if it is simulated

	vector<struct IO_elem*> outputs; // connected outputs (O IO) TODO: save as connection with connected face maybe later =)

	bool renderable;
	uint faces[3]; //store the faces while rendering to reset after it	



	IO_elem(){};
	IO_elem(cube *_c, int _x,int _y,int _z, unsigned int _type, bool _default_state)
	{
		c = _c;
		x=_x;
		y=_y;
		z=_z;
		type=_type;
		default_state=_default_state;
		sim_state=default_state;
		sim_step=0;
		etype = IOE_UNDEFINED;
	};
	
	//-- Oparations
	virtual void toggle()
	{
		sim_state = !sim_state;
	}

	virtual void reset()
	{
		sim_state = default_state;
		sim_step = 0;
	}

	bool has_output(int _x,int _y, int _z)
	{
		loopv(outputs) {
			if(outputs[i]->x==_x && outputs[i]->y==_y && outputs[i]->z==_z) return true;
		}
		return false;
	}

	bool has_output(cube *_c)
	{
		loopv(outputs) {
			if(outputs[i]->c==_c) return true;
		}
		return false;
	}
	
	//-- Simulation
	void sim(unsigned int step, int curtime)
	{
		if(sim_step!=step)
		{
			sim_step = step;
			sim_result(curtime);
		}
	}

	virtual void sim_result(int curtime)
	{
		sim_state = sim_outputs_or(curtime);
	}

	virtual bool sim_outputs_or(int curtime)
	{
		bool out = false;
		loopv(outputs)
		{
			outputs[i]->sim(sim_step,curtime);
			out = outputs[i]->sim_state || out;
		}
		return out;
	}

	//-- Render
	virtual uint render_result(){ return 0; }
};

struct LED:IO_elem
{
	LED (cube *_c, int _x,int _y,int _z, bool _default_state){
		c = _c;
		x=_x;
		y=_y;
		z=_z;
		type=IO_TYPE_I;
		default_state=_default_state;
		sim_state=default_state;
		sim_step=0;
		renderable = true;
		etype = IOE_LED;
	}
	
	uint render_result()
	{
		if(sim_state)
		{
			return TEX_LED_ON;
		}
		else
		{
			return TEX_LED_OFF;
		}
	}
};

struct TORCH:IO_elem
{
	TORCH (cube *_c,int _x,int _y,int _z, bool _default_state){
		c = _c;
		x=_x;
		y=_y;
		z=_z;
		type=IO_TYPE_IO;
		default_state=_default_state;
		sim_state=default_state;
		sim_step=0;
		renderable = false; //out for DEBUG
		//renderable = true;
		etype = IOE_TORCH;
	}
	
	void sim_result(int curtime){
		sim_state = !sim_outputs_or(curtime);
	}
	
	uint render_result(){ return 0; } //out for DEBUG
	/*
	uint render_result()
	{
		if(sim_state)
		{
			return TEX_TORCH_ON;
		}
		else
		{
			return TEX_TORCH_OFF;
		}
	}
	*/
};

struct BUFFER:IO_elem
{
	bool last_sim_state;

	BUFFER (cube *_c,int _x,int _y,int _z, bool _default_state){
		c = _c;
		x=_x;
		y=_y;
		z=_z;
		type=IO_TYPE_IO;
		default_state=_default_state;
		last_sim_state=sim_state=default_state;
		sim_step=0;
		renderable = false;
		etype = IOE_TORCH;
	}
	
	void sim_result(int curtime){
		sim_state = last_sim_state;
		last_sim_state = sim_outputs_or(curtime);
	}
	
	uint render_result(){ return 0; }
};

struct SWITCH:IO_elem
{
	SWITCH (cube *_c,int _x,int _y,int _z, bool _default_state){
		c = _c;
		x=_x;
		y=_y;
		z=_z;
		type=IO_TYPE_O;
		default_state=_default_state;
		sim_state=default_state;
		sim_step=0;
		renderable = true;
		etype = IOE_SWITCH;
	}	
	
	void sim_result(int curtime){}
	
	uint render_result()
	{
		if(sim_state)
		{
			return TEX_SWITCH_ON;
		}
		else
		{
			return TEX_SWITCH_OFF;
		}
	}
};

struct RADIO_BUTTON:IO_elem
{
	RADIO_BUTTON (cube *_c,int _x,int _y,int _z, bool _default_state){
		c = _c;
		x=_x;
		y=_y;
		z=_z;
		type=IO_TYPE_IO;
		default_state=_default_state;
		sim_state=default_state;
		sim_step=0;
		renderable = true;
		etype = IOE_RADIO_BUTTON;
	}	
	
	virtual void sim_result(int curtime){}
	virtual void toggle()
	{
		sim_state = !sim_state;
		if(sim_state){
			loopv(outputs) {
				outputs[i]->sim_state = false;
			}
		}
	}
	
	uint render_result()
	{
		if(sim_state)
		{
			return TEX_RADIO_BUTTON_ON;
		}
		else
		{
			return TEX_RADIO_BUTTON_OFF;
		}
	}
};

struct RECTANGLE_SIGNAL:IO_elem
{
	int high_time;
	int all_time; // low time + high time
	int remain_time;

	RECTANGLE_SIGNAL (cube *_c,int _x,int _y,int _z, bool _default_state){
		c = _c;
		x=_x;
		y=_y;
		z=_z;
		type=IO_TYPE_O;
		default_state=_default_state;
		sim_state=default_state;
		sim_step=0;
		renderable = false;
		etype = IOE_RECTANGLE_SIGNAL;
		all_time = 0;
	}	
	
	void sim_result(int curtime){
		if(all_time==0)
		{
			sim_state = false;
		}
		else
		{
			remain_time -= curtime;
			if(remain_time<=0) remain_time = all_time;
			if(remain_time>high_time)
			{
				sim_state = false;
			}
			else
			{
				sim_state = true;
			}

		}
	}

	void set_time(int htime,int ltime)
	{
		high_time = htime;
		all_time = high_time + ltime;
		remain_time = all_time;
	}
};


struct BUTTON:IO_elem
{
	int high_time,remain_time;

	BUTTON (cube *_c,int _x,int _y,int _z, bool _default_state){
		c = _c;
		x=_x;
		y=_y;
		z=_z;
		type=IO_TYPE_O;
		default_state=_default_state;
		sim_state=default_state;
		sim_step=0;
		renderable = true;
		etype = IOE_BUTTON;
		high_time = 500; // default 500 ms high
		remain_time = (sim_state ? high_time : 0);
	}	
	
	void toggle()
	{
		sim_state = !sim_state;
		remain_time = (sim_state ? high_time : 0);
	}

	void set_time(int htime)
	{
		high_time = htime;
		remain_time = (sim_state ? high_time : 0);
	}

	void sim_result(int curtime){
		if(remain_time<=0)
		{
			sim_state = false;
		}
		else
		{
			remain_time -= curtime;
			sim_state = true;
		}
	}
	
	uint render_result()
	{
		if(sim_state)
		{
			return TEX_BUTTON_ON;
		}
		else
		{
			return TEX_BUTTON_OFF;
		}
	}
};

vector <IO_elem*> elements;
vector <IO_elem*> I_elements;
#define loopelements() loopv(elements)
#define loopielements() loopv(I_elements)

IO_elem *findelement(int x,int y,int z){
	loopelements()
	{
		if(elements[i]->x==x && elements[i]->y==y && elements[i]->z==z) return elements[i];
	}
	return NULL;
}

bool is_high_io_element(int x,int y,int z){
	IO_elem *e = findelement(x,y,z);
	if(e){
		return e->sim_state;
	}
	return false;
}

bool is_io_element(int x,int y,int z){
	IO_elem *e = findelement(x,y,z);
	if(e) return true;
	return false;
}

int findelement_id(int x,int y,int z){
	loopelements()
	{
		if(elements[i]->x==x && elements[i]->y==y && elements[i]->z==z) return i;
	}
	return -1;
}

IO_elem *findelement(cube *_c){
	loopelements()
	{
		if(elements[i]->c==_c) return elements[i];
	}
	return NULL;
}

int findelement_id(cube *_c){
	loopelements()
	{
		if(elements[i]->c==_c) return i;
	}
	return -1;
}

void settimes_elem(int htime, int ltime,IO_elem *e){
	if(e->etype == IOE_RECTANGLE_SIGNAL)
	{
		((RECTANGLE_SIGNAL*)e)->set_time(htime,ltime);
	}
	else if(e->etype == IOE_BUTTON)
	{
		((BUTTON*)e)->set_time(htime);
	}
	else
	{
		conoutf("\f1No timing element!\f1");
	}
}

void gettimes_elem(IO_elem *e){
	if(e->etype == IOE_RECTANGLE_SIGNAL)
	{
		conoutf("htime: %d; alltime: %d",((RECTANGLE_SIGNAL*)e)->high_time,((RECTANGLE_SIGNAL*)e)->all_time);
	}
	else if(e->etype == IOE_BUTTON)
	{
		conoutf("htime: %d",((BUTTON*)e)->high_time);
	}
	else
	{
		conoutf("\f1No timing element!\f1");
	}
}


/* Commands */

void lreset(){
	loopelements() elements[i]->reset();
	simulating_step = 0;
}
COMMAND(lreset,""); // reset states of elements

#define loopsel(f){ loopi(sel.s.x) loopj(sel.s.y) loopk(sel.s.z) { int x=sel.o.x+i*sel.grid; int y=sel.o.y+j*sel.grid; int z=sel.o.z+k*sel.grid; cube *c = &lookupcube(x,y,z,sel.grid); f; }}

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
vector <wiredata*> wire_parts_done;
bool isWireDone(int x,int y,int z)
{
	loopv(wire_parts_done)
	{
		if(wire_parts_done[i]->x==x && wire_parts_done[i]->y==y && wire_parts_done[i]->z==z) return true;
	}
	return false;
}

void wire_step(int ei,int wi,int faces=6){
	cube *c;
	IO_elem *e;

	int nx,ny,nz;
	// check sorounding if wire or connected output
	loopi(faces){

		nx = current_wire_parts[wi]->x + xi[i]*circsel.grid;
		ny = current_wire_parts[wi]->y + yi[i]*circsel.grid;
		nz = current_wire_parts[wi]->z + zi[i]*circsel.grid;
		c = &lookupcube(nx,ny,nz, circsel.grid);
		

		// if wirepart not done and not empty
		if (!isempty(*c) && !isWireDone(nx,ny,nz))
		{
			e = findelement(c);
			if(e)
			{
				//IO on bottom or Output, connect with current element
				if((e->type == IO_TYPE_IO && i==O_BOTTOM) || (e->type == IO_TYPE_O))
				{
					elements[ei]->outputs.add(e);
					wire_parts_done.add(new wiredata(nx,ny,nz)); // wire part done for this element
				}
			}
			// one way wire
			else if((c->texture[O_BOTTOM] == TEX_WIRE_ON || c->texture[O_BOTTOM] == TEX_WIRE_ONE_WAY) && c->texture[of[i]] != TEX_WIRE_ONE_WAY)
			{	
				// wire
				current_wire_parts.add(new wiredata(nx,ny,nz)); // add to wire parts to be checked in next step
				wire_parts_done.add(new wiredata(nx,ny,nz));  // wire part done for this element
			}
		}
	}
	// current wirepart ready delete from wireparts to check
	delete current_wire_parts[wi];
	current_wire_parts.remove(wi);
}

vector <IO_elem*> IO_renderables;
#define looprelements() loopv(IO_renderables)
bool scaned = false;

void lclear()
{
	scaned = false;
	looprelements()
	{
		loopj(3){
			IO_renderables[i]->c->faces[j] = IO_renderables[i]->faces[j];
		}
	}
	elements.deletecontents();
	I_elements.setsize(0);
	IO_renderables.setsize(0);
	changed(circsel);
}
COMMAND(lclear,""); // clear circuit

bool is_tex_cube(cube *c,const int tex)
{
	if(!isempty(*c))
	{
		if(c->texture[O_BOTTOM] == tex)
		{
			return true;
		}
	}
	return false;
}

void init_element_times(IO_elem *e)
{
	int htime = 0;
	int ltime = 0;

	cube *c;
	int x,y,z;
	int nx,ny;
	loopi(4)
	{
		// only one direction on xy plane
		x = e->x + xi[i]*circsel.grid;
		y = e->y + yi[i]*circsel.grid;
		z = e->z;
		c = &lookupcube(x,y,z, circsel.grid);

		if(is_tex_cube(c,TEX_TIMER))
		{
			// Timer input found
			// max 16 bit TODO: maybe max int size / 20ms (times are stored in int)
			loopk(16)
			{
				nx = x + k*xi[i]*circsel.grid;
				ny = y + k*yi[i]*circsel.grid;
				c = &lookupcube(nx,ny,z, circsel.grid);

				if(is_tex_cube(c,TEX_TIMER))
				{
					// look if has a high and a low
					// 20ms minimal time
					c = &lookupcube(nx,ny,z+circsel.grid, circsel.grid);
					if(is_tex_cube(c,TEX_TIME_HIGH)) htime += 1<<k;
					c = &lookupcube(nx,ny,z-circsel.grid, circsel.grid);
					if(is_tex_cube(c,TEX_TIME_LOW)) ltime += 1<<k;
				}else{
					break;
				}
			}
		}
	}
	if(htime!=0 || ltime!=0) settimes_elem(htime, ltime,e);
}

int logic_progress = 0;
int logic_progress_total = 0;
void lscan() {
	//if (sel.grid !=4) {conoutf ("\f1#circuit:\f2 please use gridsize 4 when selecting for scan."); return;}
	
	lclear();

	circsel = sel;
	// collect elements
	loopsel(
		if(!isempty(*c))
		{
			switch(c->texture[O_BOTTOM])
			{
				case TEX_SWITCH_ON:
					elements.add(new SWITCH(c,x ,y ,z, true));
					break;
				case TEX_SWITCH_OFF:
					elements.add(new SWITCH(c,x ,y ,z, false));
					break;
				case TEX_TORCH_ON:
					elements.add(new TORCH(c,x ,y ,z, true));
					break;	
				case TEX_TORCH_OFF:
					elements.add(new TORCH(c,x ,y ,z, false));
					break;
				case TEX_LED_ON:
					I_elements.add(elements.add(new LED(c,x ,y ,z, true))); //I elements to I_elements for simulation (only I not IO)
					break;
				case TEX_LED_OFF:
					I_elements.add(elements.add(new LED(c,x ,y ,z, false))); //I elements to I_elements for simulation (only I not IO)
					break;
				case TEX_RECTANGLE_SIGNAL:
					init_element_times(elements.add(new RECTANGLE_SIGNAL(c,x ,y ,z, false)));
					break;
				case TEX_BUTTON_ON:
					init_element_times(elements.add(new BUTTON(c,x ,y ,z, true)));
					break;
				case TEX_BUTTON_OFF:
					init_element_times(elements.add(new BUTTON(c,x ,y ,z, false)));
					break;
				case TEX_RADIO_BUTTON_ON:
					elements.add(new RADIO_BUTTON(c,x ,y ,z, true));
					break;
				case TEX_RADIO_BUTTON_OFF:
					elements.add(new RADIO_BUTTON(c,x ,y ,z, false));
					break;
				case TEX_BUFFER:
					elements.add(new BUFFER(c,x ,y ,z, false));
					break;
			}
		}
	);
	
	conoutf("\f1%d Elements found.\f1",elements.length());

	// scann connections
	logic_progress = 0;
	logic_progress_total = elements.length();

	loopelements()
	{
		// only input elements connections are relevant
		if(elements[i]->type==IO_TYPE_IO || elements[i]->type==IO_TYPE_I){
			// add current element to wire done and wire parts
			current_wire_parts.add(new wiredata(elements[i]->x,elements[i]->y,elements[i]->z));
			wire_parts_done.add(new wiredata(elements[i]->x,elements[i]->y,elements[i]->z));

			// check sourounding elements for connected output or wires
			if(elements[i]->type==IO_TYPE_IO) wire_step(i, 0, 5);//5 faces cause on top is output
			if(elements[i]->type==IO_TYPE_I) wire_step(i, 0, 6); //6 faces all faces can be input
			
			// while wires to walk check all wires sourounding for connected output or wires
			while(!current_wire_parts.empty())
			{
				for(int wi=current_wire_parts.length()-1;wi>=0;wi--){
					wire_step(i,wi);
				}
			}
			// reset wires done for next element
			wire_parts_done.deletecontents();
		}
		logic_progress++;
		renderprogress(float(logic_progress)/logic_progress_total, "scanning connections...");
	}

	// prepare renderable cubes
	loopelements()
	{
		if(elements[i]->renderable)
		{
			IO_renderables.add(elements[i]);
			loopj(3){
				elements[i]->faces[j] = elements[i]->c->faces[j];
				elements[i]->c->faces[j] = 0x71717171;
			}
		}
	}

	changed(sel);
	scaned = true;
}
COMMAND(lscan,""); // scans selection and adds new elements to circuit

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
		I_elements[i]->sim(simulating_step, 0);
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
	IO_elem *e = findelement(sel.o.x,sel.o.y,sel.o.z);
	if(e){
		e->toggle();
	}else{
		conoutf("\f1No circuit element!\f1");
	}
}
COMMAND(lset,""); //toggle a element state

void lsettimes(int *htime, int *ltime)
{
	circuitgriderr;
	IO_elem *e = findelement(sel.o.x,sel.o.y,sel.o.z);
	if(e){
		settimes_elem(*htime, *ltime,e);
	}else{
		conoutf("\f1No circuit element!\f1");
	}
}
COMMAND(lsettimes,"ii");

void lgettimes()
{
	circuitgriderr;
	IO_elem *e = findelement(sel.o.x,sel.o.y,sel.o.z);
	if(e){
		gettimes_elem(e);
	}else{
		conoutf("\f1No circuit element!\f1");
	}
}
COMMAND(lgettimes,"");

VAR(lpause, 0, 0, 1);

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
COMMAND(lbuildtimer,"ii"); // build a timer input in ms smallest step 20ms

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
VARP(ltimestep, 1, 1, 3000);
int curr_time = 0;

void updatelogicsim(int curtime)
{
	if(!scaned || curtime==0 || lpause) return;

	// ms loop
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