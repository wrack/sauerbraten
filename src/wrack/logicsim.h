#include "game.h"
//add frequenz sound to sound gamesounds
extern int addsoundfrequenz(double f, Uint16 *buffer);

//max frequncy of timers
const uint MAX_TIMER_FREQUENCY = 16; // max 16 bit TODO: maybe max int size

// texture index for scanning and rendering io elements
const uint TEX_WIRE_ON = 834;
const uint TEX_WIRE_ONE_WAY = 832;
const uint TEX_SWITCH_ON = 456;
const uint TEX_SWITCH_OFF = 417;
const uint TEX_SWITCH_COLLIDE_ON = 210;
const uint TEX_SWITCH_COLLIDE_OFF = 504;
const uint TEX_TORCH_ON = 774;
const uint TEX_TORCH_OFF = 776;
const uint TEX_LED_ON = 764;
const uint TEX_LED_OFF = 814;
const uint TEX_BUTTON_ON = 464;
const uint TEX_BUTTON_OFF = 418;
const uint TEX_RADIO_BUTTON_OFF = 113;
const uint TEX_RADIO_BUTTON_ON = 201;
const uint TEX_BUFFER = 780;
const uint TEX_RECTANGLE_SIGNAL_SWITCH = 593;

const uint TEX_RECTANGLE_SIGNAL = 833;
const uint TEX_TIMER = 784;
const uint TEX_TIME_HIGH = 832;
const uint TEX_TIME_LOW = 831;

const uint TEX_SPEAKER = 711;
const uint TEX_PUNCHCARD = 785;
const uint TEX_INOUT_BIT = 771;
const uint TEX_OUT_BIT_OFF = 804;
const uint TEX_OUT_BIT_ON = 768;

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
	IOE_RECTANGLE_SIGNAL_SWITCH,
	IOE_SWITCH_COLLIDE,
	IOE_SPEAKER,
	IOE_PUNCHCARD,
	IOE_INOUT_BIT,
	IOE_OUT_BIT,
	IOE_BUFFER,
};

/* IO Elements */

struct IO_elem
{
	unsigned int type; // see IO_types
	unsigned int etype; // see IO_elem_type
	int x,y,z;
	bool sim_state; // high or low
	unsigned int sim_step; // to checks if it is simulated

	vector<struct IO_elem*> outputs; // connected outputs (O IO) TODO: save as connection with connected face maybe later =)

	//separate this in renderable io
	uint faces[3]; //store the faces while rendering to reset after it	

	IO_elem(){};
	IO_elem(int _x, int _y, int _z, bool _state, uint _type, uint _etype)
	{
		x = _x;
		y = _y;
		z = _z;
		type=_type;
		etype = _etype;
		sim_state=_state;
		sim_step=0;
		
	};

	IO_elem(uint _type, uint _etype)
	{
		type=_type;
		etype = _etype;
	};
	
	//-- Oparations
	virtual void toggle(){ sim_state = !sim_state; }

	virtual void reset(){ sim_step = 0;	}
	
	//-- Simulation
	void sim(unsigned int step, int curtime)
	{
		if(sim_step!=step)
		{
			sim_step = step;
			sim_result(curtime);
		}
	}

	virtual void sim_result(int curtime){ sim_state = sim_outputs_or(curtime);	}

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

struct INOUT_BIT:IO_elem
{
	int index;
	
	INOUT_BIT(){};
	INOUT_BIT (int _x, int _y, int _z, bool _state):IO_elem(_x, _y, _z, _state, IO_TYPE_IO, IOE_INOUT_BIT){}	
};

struct OUT_BIT:IO_elem
{
	int index;
	
	OUT_BIT(){};
	OUT_BIT (int _x, int _y, int _z, bool _state):IO_elem(_x, _y, _z, _state, IO_TYPE_O, IOE_OUT_BIT){}

	void sim_result(int curtime){ sim_outputs_or(curtime); } // sim outputs cause punchcard could be one
};

struct IO_elem_timer_inoutbitable:IO_elem
{
	bool has_hbits, has_lbits;
	
	vector <INOUT_BIT*> hbits; // frequenz high bits
	vector <INOUT_BIT*> lbits; // frequenz low bits
	
	IO_elem_timer_inoutbitable(){}
	IO_elem_timer_inoutbitable(int _x, int _y, int _z, bool _state, uint _type, uint _etype):IO_elem(_x, _y, _z, _state, _type, _etype)
	{
		has_hbits = has_lbits = false; 
	}

	virtual void scan_inout_bits(int curtime){}

	virtual void sim_outbits(int curtime){
		loopv(hbits) hbits[i]->sim(sim_step,curtime);
		loopv(lbits) lbits[i]->sim(sim_step,curtime);
	}

	virtual void add_hinout_bit(INOUT_BIT* outbit){
		hbits.add(outbit);
		has_hbits = true;
	}

	virtual void add_linout_bit(INOUT_BIT* outbit){
		lbits.add(outbit);
		has_lbits = true;
	}

	virtual bool has_inout_bits(){
		return (has_hbits || has_lbits);
	}
};

struct LED:IO_elem
{
	LED (int _x, int _y, int _z, bool _state):IO_elem(_x, _y, _z, _state, IO_TYPE_I, IOE_LED){}
	
	uint render_result()
	{
		return sim_state ? TEX_LED_ON : TEX_LED_OFF;
	}
};

struct SPEAKER:IO_elem_timer_inoutbitable
{
	int sid;
	int chanel;
	Uint16 *buffer;
	bool plays;
	double frequenz;

	SPEAKER (int _x, int _y, int _z, bool _state):IO_elem_timer_inoutbitable(_x, _y, _z, _state, IO_TYPE_I, IOE_SPEAKER)
	{
		sid = chanel = -1;
		plays = false;
		frequenz = 0;
	}
	
	void sim_result(int curtime)
	{
		if(has_inout_bits()) scan_inout_bits(curtime);
		if(frequenz==0) return;
		sim_state = sim_outputs_or(curtime);
		if(sid>=0){
			if(sim_state && !plays) play();
			if(!sim_state && plays) stop();
		}
	}

	void set_frequenz(double freq)
	{
		frequenz = freq;
		stop();
		if(frequenz == 0) return;
		sid = addsoundfrequenz(frequenz ,buffer);
	}

	void play()
	{
		chanel = playsound(sid, &vec(x,y,z), NULL, -1, 0, chanel, 200);
		plays = true;
	}

	void stop()
	{
		stopsound(sid,chanel,0);
		chanel = -1;
		plays = false;
	}

	void scan_inout_bits(int curtime)
	{
		sim_outbits(curtime);
		int hz = 0;
		int mhz = 0;
		double freq = 0;
		
		loopi(MAX_TIMER_FREQUENCY){
			if(has_hbits && hbits.inrange(i)) hz += (int)hbits[i]->sim_state<<hbits[i]->index;
			if(has_lbits && lbits.inrange(i)) mhz += (int)lbits[i]->sim_state<<lbits[i]->index;
		}
		freq = (double)hz + (double)mhz/1000;
		if(freq != frequenz) set_frequenz(freq);
	}
};

struct TORCH:IO_elem
{
	TORCH (int _x, int _y, int _z, bool _state):IO_elem(_x, _y, _z, _state, IO_TYPE_IO, IOE_TORCH){}
	
	void sim_result(int curtime){ sim_state = !sim_outputs_or(curtime);	}
};

struct BUFFER:IO_elem
{
	bool last_sim_state;

	BUFFER (int _x, int _y, int _z, bool _state):IO_elem(_x, _y, _z, _state, IO_TYPE_IO, IOE_BUFFER)
	{
		last_sim_state = false;
	}
	
	void sim_result(int curtime){
		sim_state = last_sim_state;
		last_sim_state = sim_outputs_or(curtime);
	}
};

struct SWITCH:IO_elem
{
	SWITCH(){};
	SWITCH (int _x, int _y, int _z, bool _state):IO_elem(_x, _y, _z, _state, IO_TYPE_O, IOE_SWITCH){}	
	SWITCH (int _x, int _y, int _z, bool _state, uint _etype):IO_elem(_x, _y, _z, _state, IO_TYPE_O, _etype){}	
	
	void sim_result(int curtime){ } 
	
	uint render_result()
	{
		return sim_state ? TEX_SWITCH_ON : TEX_SWITCH_OFF;
	}
};

struct SWITCH_COLLIDE:SWITCH{
	SWITCH_COLLIDE (int _x, int _y, int _z, bool _state):SWITCH(_x, _y, _z, _state, IOE_SWITCH_COLLIDE){}

	uint render_result()
	{
		return sim_state ? TEX_SWITCH_COLLIDE_ON : TEX_SWITCH_COLLIDE_OFF;
	}
};

struct RADIO_BUTTON:IO_elem
{
	RADIO_BUTTON (int _x, int _y, int _z, bool _state):IO_elem(_x, _y, _z, _state, IO_TYPE_IO, IOE_RADIO_BUTTON){}	
	
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
		return sim_state ?  TEX_RADIO_BUTTON_ON : TEX_RADIO_BUTTON_OFF;
	}
};

struct RECTANGLE_SIGNAL:IO_elem_timer_inoutbitable
{
	int high_time, low_time;
	int next_htime, next_ltime;
	int all_time; // low time + high time
	int remain_time;

	RECTANGLE_SIGNAL(){};
	RECTANGLE_SIGNAL (int _x, int _y, int _z, bool _state):IO_elem_timer_inoutbitable(_x, _y, _z, _state, IO_TYPE_O, IOE_RECTANGLE_SIGNAL)
	{
		init();
	}
	RECTANGLE_SIGNAL (int _x, int _y, int _z, bool _state, uint _type, uint _etype):IO_elem_timer_inoutbitable(_x, _y, _z, _state, _type, _etype)
	{
		init();
	}

	void init()
	{
		all_time = high_time = low_time = 0;
		next_htime = next_ltime = 0;
		remain_time = -1;
	}
	
	void sim_result(int curtime){
		if(has_inout_bits()) scan_inout_bits(curtime);
		if(nosim()) return;
		
		time_step(curtime);
		sim_state = !(remain_time>high_time);
	}

	void time_step(int curtime)
	{
		remain_time -= curtime;
		if(remain_time < 0)
		{
			set_time(next_htime, next_ltime);
			remain_time = all_time;
		}
	}

	void set_time(int htime,int ltime)
	{
		high_time = next_htime = htime;
		low_time = next_ltime = ltime;
		all_time = high_time + low_time;
		remain_time = -1;
	}

	bool nosim()
	{
		if(all_time==0 && next_htime == 0 && next_ltime == 0)
		{
			reset_elem();
			return true;
		}
		return false;
	}

	void reset_elem()
	{
		sim_state = false;
		remain_time = -1;
	}

	void scan_inout_bits(int curtime)
	{
		sim_outbits(curtime);
		int htime = has_hbits ? 0 : high_time;
		int ltime = has_lbits ? 0 : low_time;
		
		loopi(MAX_TIMER_FREQUENCY){
			if(has_hbits && hbits.inrange(i)) htime += (int)hbits[i]->sim_state<<hbits[i]->index;
			if(has_lbits && lbits.inrange(i)) ltime += (int)lbits[i]->sim_state<<lbits[i]->index;
		}
		
		if(htime != high_time || ltime != low_time)
		{		
			next_htime = htime;
			next_ltime = ltime;
		}
	}
};
//TODO rect sig allways false at init
struct RECTANGLE_SIGNAL_SWITCH:RECTANGLE_SIGNAL
{
	RECTANGLE_SIGNAL_SWITCH (int _x, int _y, int _z, bool _state):RECTANGLE_SIGNAL(_x, _y, _z, _state, IO_TYPE_IO, IOE_RECTANGLE_SIGNAL_SWITCH){}

	void sim_result(int curtime){
		if(has_inout_bits()) scan_inout_bits(curtime);
		if(nosim()) return;

		if(sim_outputs_or(curtime))
		{
			time_step(curtime);
			sim_state = !(remain_time>high_time);
		}
		else
		{
			reset_elem();
		}
	}
};

struct BUTTON:IO_elem
{
	int high_time,remain_time;

	BUTTON (int _x, int _y, int _z, bool _state):IO_elem(_x, _y, _z, _state, IO_TYPE_O, IOE_BUTTON)
	{
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
		sim_outputs_or(curtime); // sim outputs cause punchcad could be one
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
		return sim_state ? TEX_BUTTON_ON : TEX_BUTTON_OFF;
	}
};

struct PUNCHCARD:IO_elem
{
	// outputs is 0 is forth bit and 1 is back bit
	vector <bool> card; //data
	vector <IO_elem*> inputs; // elements to set data (switch)
	vector <OUT_BIT*> row; // elements to set data to show the current row
	int currstep, currdata;
	int width, length;
	bool last_fstate, fstate;
	bool last_bstate, bstate;

	PUNCHCARD ():IO_elem(IO_TYPE_IO, IOE_PUNCHCARD)
	{
		currstep = currdata = 0;
		width = length = 0;
		last_fstate = fstate = last_bstate = bstate = false;
	}	

	void sim_result(int curtime){
		
		if(width>0){
			//forth
			outputs[0]->sim(sim_step,curtime);
			fstate = outputs[0]->sim_state;
			if(!last_fstate && fstate) step_forth();

			//back
			if(outputs.inrange(1))
			{
				outputs[1]->sim(sim_step,curtime);
				bstate = outputs[1]->sim_state;
				if(!last_bstate && bstate) step_back();
			}
			
			loopi(width) inputs[i]->sim_state = card[currdata+i];
			loopv(row) row[i]->sim_state = (currstep==row[i]->index) ? true : false;

			last_fstate = fstate;
			last_bstate = bstate;
		}
		
	}

	void step_forth()
	{
		currstep ++;
		if(currstep == length) currstep = 0;
		currdata = currstep*width;
	}

	void step_back()
	{
		currstep --;
		if(currstep < 0) currstep = length - 1;
		currdata = currstep*width;
	}
};

