//***********************************************************************************************
//Bi-dimensional multimixer module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"


// http://www.earlevel.com/main/2012/12/15/a-one-pole-filter/
struct OnePoleFilter {
    float b1 = 0.0;
	float lowout = 0.0;
	float lastin = 0.0f;
	
    void setCutoff(float Fc) {
		b1 = exp(-2.0 * M_PI * Fc);
	}
    void process(float in) {
		lastin = in;
		lowout = in * (1.0f - b1) + lowout * b1;
	}
	float lowpass() {
		return lowout;
	}
	float highpass() {
		return lastin - lowout;
	}
};


// This struct is an SIMD adaptation by Marc Boulé of the filter from
// http://www.earlevel.com/main/2012/12/15/a-one-pole-filter/
struct QuadOnePoleFilter {
	simd::f32_4 b1 = simd::f32_4(0.0f);
	simd::f32_4 lowout = simd::f32_4(0.0f);
	float lowpassOuts[4] = {0.0f};
	float highpassOuts[4] = {0.0f};	
	
    void setCutoff(int index, float Fc) {
		float tb1[4] = {0.0f};
		b1.store(tb1);
		tb1[index] = std::exp(-2.0 * M_PI * Fc);
		b1 = simd::f32_4::load(tb1);
	}
	
    void process(float *inflt) {
		simd::f32_4 lastin = simd::f32_4::load(inflt);
		lowout = lastin * (1.0f - b1) + lowout * b1;
		lowout.store(lowpassOuts);
		lastin -= lowout;
		lastin.store(highpassOuts);
	}
	
	float lowpass(int index) {
		return lowpassOuts[index];
	}
	float highpass(int index) {
		return highpassOuts[index];
	}
};


/*
struct RCFilter {
	float c = 0.f;
	float xstate[1] = {};
	float ystate[1] = {};

	// `r` is the ratio between the cutoff frequency and sample rate, i.e. r = f_c / f_s
	void setCutoff(float r) {
		c = 2.f / r;
	}
	void process(float x) {
		float y = (x + xstate[0] - ystate[0] * (1 - c)) / (1 + c);
		xstate[0] = x;
		ystate[0] = y;
	}
	float lowpass() {
		return ystate[0];
	}
	float highpass() {
		return xstate[0] - ystate[0];
	}
};
*/


// This struct is an SIMD adaptation by Marc Boulé of Andrew Belt's dsp::RCFilter code in VCV Rack
struct QuadRCFilter {
	float cutoffs[4] = {0.0f};// do not change here, constructor will use to init x,y states
	simd::f32_4 xstate = simd::f32_4(0.0f);
	simd::f32_4 ystate = simd::f32_4(0.0f);
	float lowpassOuts[4] = {0.0f};
	float highpassOuts[4] = {0.0f};

	// `r` is the ratio between the cutoff frequency and sample rate, i.e. r = f_c / f_s
	void setCutoff(int index, float r) {// TODO add sample rate to calculation so that a cutoff f_c is not changed when sample rate changes
		cutoffs[index] = 2.f / r;
	}
	void process(float *x_f) {// argument: 4 inputs to quad filter
		simd::f32_4 c = simd::f32_4::load(cutoffs);
		simd::f32_4 x = simd::f32_4::load(x_f);
		simd::f32_4 y = (x + xstate - ystate * (1.0f - c)) / (1.0f + c);
		xstate = x;
		ystate = y;
		y.store(lowpassOuts);
		x -= y;
		x.store(highpassOuts);
	}
	float lowpass(int index) {
		return lowpassOuts[index];
	}
	float highpass(int index) {
		return highpassOuts[index];
	}
};



// This struct is an adaptation by Marc Boulé of Andrew Belt's dsp::RCFilter code in VCV Rack
struct QuadRCFilter2 {
	float c[4] = {0.f};
	float xstate[4] = {};
	float ystate[4] = {};

	// `r` is the ratio between the cutoff frequency and sample rate, i.e. r = f_c / f_s
	void setCutoff(int index, float r) {
		c[index] = 2.f / r;
	}
	void process(float *x) {
		float y[4];
		for (int i = 0; i < 4; i++) {
			y[i] = (x[i] + xstate[i] - ystate[i] * (1 - c[i])) / (1 + c[i]);
			xstate[i] = x[i];
			ystate[i] = y[i];
		}
	}
	float lowpass(int index) {
		return ystate[index];
	}
	float highpass(int index) {
		return xstate[index] - ystate[index];
	}
};
	

//*****************************************************************************


struct chanVol {// a mixMap for an output has four of these, for each quadrant that can map to its output
	float vol;// 0.0 to 1.0
	float chan;// channel input number (0 to 15), garbage when vol is 0.0
	bool inputIsAboveOutput;// true when an input is located above the output, false otherwise
};


struct mixMapOutput {
	chanVol cvs[4];// an output can have a mix of at most 4 inputs
	QuadOnePoleFilter filt;
	// QuadRCFilter filt;

	float calcFilteredOutput() {
		float filterOut = 0.0f;
		for (int i = 0; i < 4; i++) {
			if (cvs[i].vol > 0.0f) {
				filterOut += (cvs[i].inputIsAboveOutput ? filt.highpass(i) : filt.lowpass(i));
			}				
		}
		return filterOut;
	}

	void init() {
		for (int i = 0; i < 4; i++) {
			cvs[i].vol = 0.0f;// when this is non-zero, .chan and .inputIsAboveOutput are assuredly valid
		}	
	}
	
	void insert(int numerator, int denominator, int mixmode, float _chan, bool _inAboveOut) {
		float _vol = (mixmode == 1 ? 1.0f : ((float)numerator / (float)denominator)); 
		for (int i = 0; i < 4; i++) {
			if (cvs[i].vol == 0.0f) {
				cvs[i].vol = _vol;
				cvs[i].chan = _chan;
				cvs[i].inputIsAboveOutput = _inAboveOut;
				float f_c = (float)calcCutoffFreq(numerator, denominator, _inAboveOut);
				filt.setCutoff(i, f_c / 44100.0f);
				return;
			}
		}
		assert(false);
	}
	
	void processFilters(float *invals) {
		filt.process(invals);
	}
	
	inline int calcCutoffFreq(int num, int denum, bool isHighPass) {
		switch (denum) {
			case (3) :
				if (num == 1) return isHighPass ? 8000 : 60;
				return isHighPass ? 3000 : 200;
			break;
			
			case (4) :
				if (num == 1) return isHighPass ? 10000 : 50;
				if (num == 3) return isHighPass ? 3900 : 250;
			break;
			
			case (5) :
				if (num == 1) return isHighPass ? 12000 : 40;
				if (num == 2) return isHighPass ? 7000 : 70;
				if (num == 3) return isHighPass ? 4000 : 150;
				return isHighPass ? 3000 : 250;
			break;
			
			case (6) :
				if (num == 1) return isHighPass ? 12500 : 30;
				if (num == 2) return isHighPass ? 8000 : 60;
				if (num == 4) return isHighPass ? 3000 : 200;
				if (num == 5) return isHighPass ? 2500 : 300;
			break;
			
			case (7) :
				if (num == 1) return isHighPass ? 15000 : 27;
				if (num == 2) return isHighPass ? 8100 : 50;
				if (num == 3) return isHighPass ? 6000 : 80;
				if (num == 4) return isHighPass ? 4000 : 125;
				if (num == 5) return isHighPass ? 2500 : 200;
				return isHighPass ? 2000 : 350;
			break;
			
			case (8) :
				if (num == 1) return isHighPass ? 18000 : 22;
				if (num == 2) return isHighPass ? 10000 : 40;
				if (num == 3) return isHighPass ? 7000 : 68;
				if (num == 5) return isHighPass ? 3900 : 160;
				if (num == 6) return isHighPass ? 2500 : 250;
				if (num == 7) return isHighPass ? 1500 : 400;
			break;
		}
		return isHighPass ? 5000 : 100;
	}
};
	

//*****************************************************************************


struct Torus : Module {
	enum ParamIds {
		GAIN_PARAM,
		MODE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(MIX_INPUTS, 16),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(MIX_OUTPUTS, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		DECAY_LIGHT,
		CONSTANT_LIGHT,
		FILTER_LIGHT,
		NUM_LIGHTS
	};
	
	
	// Constants
	// none
	
	// Need to save
	int panelTheme;
	int mixmode;// 0 is decay, 1 is constant, 2 is filter
	
	// No need to save
	unsigned int lightRefreshCounter = 0;
	Trigger modeTrigger;
	mixMapOutput mixMap[7];// 7 outputs
	
	
	Torus() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(Torus::MODE_PARAM, 0.0f, 1.0f, 0.0f, "Mode");
		configParam(Torus::GAIN_PARAM, 0.0f, 2.0f, 1.0f, "Gain");

		onReset();

		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		mixmode = 0;
		updateMixMap();
	}


	void onSampleRateChange() override {
		updateMixMap();// this will make sure cutoff freqs get adjusted if necessary
	}		
	
	
	void onRandomize() override {
	}
	

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// mixmode
		json_object_set_new(rootJ, "mixmode", json_integer(mixmode));

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// mixmode
		json_t *mixmodeJ = json_object_get(rootJ, "mixmode");
		if (mixmodeJ)
			mixmode = json_integer_value(mixmodeJ);
	}

	void process(const ProcessArgs &args) override {		
		// user inputs
		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {
			// mixmode
			if (modeTrigger.process(params[MODE_PARAM].getValue())) {
				if (++mixmode > 2)
					mixmode = 0;
			}
			
			updateMixMap();
		}// userInputs refresh
		
		
		// mixer code
		for (int outi = 0; outi < 7; outi++) {
			float outValue = outputs[MIX_OUTPUTS + outi].isConnected() ? clamp(calcOutput(outi) * params[GAIN_PARAM].getValue(), -10.0f, 10.0f) : 0.0f;
			outputs[MIX_OUTPUTS + outi].setVoltage(outValue);
		}
		

		// lights
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;

			lights[DECAY_LIGHT].setBrightness(mixmode == 0 ? 1.0f : 0.0f);
			lights[CONSTANT_LIGHT].setBrightness(mixmode == 1 ? 1.0f : 0.0f);
			lights[FILTER_LIGHT].setBrightness(mixmode == 2 ? 1.0f : 0.0f);
		}// lightRefreshCounter
	}// step()
	
	
	void updateMixMap() {
		for (int outi = 0; outi < 7; outi++) {
			mixMap[outi].init();
		}
		
		// scan inputs for upwards flow (input is below output)
		int distanceUL = 1;
		int distanceUR = 1;
		for (int ini = 1; ini < 8; ini++) {
			distanceUL++;
			distanceUR++;
			
			// left side
			if (inputs[MIX_INPUTS + ini].isConnected()) {
				for (int outi = ini - 1 ; outi >= 0; outi--) {
					int numerator = (distanceUL - ini + outi);
					if (numerator == 0) 
						break;
					mixMap[outi].insert(numerator, distanceUL, mixmode, ini, false);
				}
				distanceUL = 1;
			}
			
			// right side
			if (inputs[MIX_INPUTS + 8 + ini].isConnected()) {
				for (int outi = ini - 1 ; outi >= 0; outi--) {
					int numerator = (distanceUR - ini + outi);
					if (numerator == 0) 
						break;
					mixMap[outi].insert(numerator, distanceUL, mixmode, 8 + ini, false);
				}
				distanceUR = 1;
			}			
		}	

		// scan inputs for downward flow (input is above output)
		int distanceDL = 1;
		int distanceDR = 1;
		for (int ini = 6; ini >= 0; ini--) {
			distanceDL++;
			distanceDR++;
			
			// left side
			if (inputs[MIX_INPUTS + ini].isConnected()) {
				for (int outi = ini ; outi < 7; outi++) {
					int numerator = (distanceDL - 1 + ini - outi);
					if (numerator == 0) 
						break;
					mixMap[outi].insert(numerator, distanceUL, mixmode, ini, true);
				}
				distanceDL = 1;
			}
			
			// right side
			if (inputs[MIX_INPUTS + 8 + ini].isConnected()) {
				for (int outi = ini ; outi < 7; outi++) {
					int numerator = (distanceDR - 1 + ini - outi);
					if (numerator == 0) 
						break;
					mixMap[outi].insert(numerator, distanceUL, mixmode, 8 + ini, true);
				}
				distanceDR = 1;
			}		
		}	
	}
	
	
	inline float calcOutput(int outi) {
		float outputValue = 0.0f;
		if (mixmode < 2) {// constant or decay modes	
			for (int i = 0; i < 4; i++) {
				 float vol = mixMap[outi].cvs[i].vol;
				 if (vol > 0.0f) {
					outputValue += inputs[MIX_INPUTS + mixMap[outi].cvs[i].chan].getVoltage() * vol;
				 }
			}
		}
		else {// filter mode
			float invals[4] = {0.0f};
			for (int i = 0; i < 4; i++) {
				float vol = mixMap[outi].cvs[i].vol;
				if (vol > 0.0f) {
					invals[i] = inputs[MIX_INPUTS + mixMap[outi].cvs[i].chan].getVoltage();
				}
			}
			mixMap[outi].processFilters(invals);
			outputValue += mixMap[outi].calcFilteredOutput();
		}
		return outputValue;
	}
};


struct TorusWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		Torus *module;
		int theme;
		void onAction(const event::Action &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Torus *module = dynamic_cast<Torus*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *lightItem = new PanelThemeItem();
		lightItem->text = lightPanelID;// Geodesics.hpp
		lightItem->module = module;
		lightItem->theme = 0;
		menu->addChild(lightItem);

		PanelThemeItem *darkItem = new PanelThemeItem();
		darkItem->text = darkPanelID;// Geodesics.hpp
		darkItem->module = module;
		darkItem->theme = 1;
		menu->addChild(darkItem);
		
		menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));
	}	
	
	TorusWidget(Torus *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/Torus-WL.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/Torus-WL.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}

		// Screws 
		// part of svg panel, no code required
		
		float colRulerCenter = box.size.x / 2.0f;

		// mixmode button
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter, 380.0f - 329.5f), module, Torus::MODE_PARAM, module ? &module->panelTheme : NULL));
		
		// decay and constant lights
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter, 380.0f - 343.5f), module, Torus::FILTER_LIGHT));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - 12.5f, 380.0f - 322.5f), module, Torus::DECAY_LIGHT));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + 12.5f, 380.0f - 322.5f), module, Torus::CONSTANT_LIGHT));

		// gain knob
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, 380 - 294), module, Torus::GAIN_PARAM, module ? &module->panelTheme : NULL));
		
		// inputs
		static const int offsetY = 34;
		for (int i = 0; i < 8; i++) {
			addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - 22.5f, 380 - (270 - offsetY * i)), true, module, Torus::MIX_INPUTS + i, module ? &module->panelTheme : NULL));
			addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + 22.5f, 380 - (270 - offsetY * i)), true, module, Torus::MIX_INPUTS + 8 + i, module ? &module->panelTheme : NULL));
		}
		
		// mix outputs
		for (int i = 0; i < 7; i++) {
			addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, 380 - (253 - offsetY * i)), false, module, Torus::MIX_OUTPUTS + i, module ? &module->panelTheme : NULL));
		}
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((Torus*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Torus*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelTorus = createModel<Torus, TorusWidget>("Torus");

/*CHANGE LOG

*/