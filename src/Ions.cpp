//***********************************************************************************************
//Atomic Duophonic Voltage Sequencer module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"


struct Ions : Module {
	enum ParamIds {
		RUN_PARAM,
		RESET_PARAM,
		ENUMS(CV_PARAMS, 15),// 0 is center, move conter clockwise top atom, then clockwise bot atom
		PROB_PARAM,
		ENUMS(OCT_PARAMS, 2),
		LEAP_PARAM,
		ENUMS(STATE_PARAMS, 2),// 3 states : global, local, global+local
		PLANK_PARAM,
		uncertainty_PARAM,
		RESETONRUN_PARAM,
		STEPCLOCKS_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLK_INPUT,
		ENUMS(CLK_INPUTS, 2),
		RUN_INPUT,
		RESET_INPUT,
		PROB_INPUT,// CV_value/10  is added to PROB_PARAM, which is a 0 to 1 knob
		ENUMS(OCTCV_INPUTS, 2),
		ENUMS(STATECV_INPUTS, 2),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(SEQ_OUTPUTS, 2),
		ENUMS(JUMP_OUTPUTS, 2),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(BLUE_LIGHTS, 16),
		ENUMS(YELLOW_LIGHTS, 16),
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GLOBAL_LIGHTS, 2),// 0 is top atom, 1 is bottom atom
		ENUMS(LOCAL_LIGHTS, 2),
		LEAP_LIGHT,
		ENUMS(OCTA_LIGHTS, 3),// 0 is center, 1 is inside mirrors, 2 is outside mirrors
		ENUMS(OCTB_LIGHTS, 3),
		PLANK_LIGHT,
		uncertainty_LIGHT,
		ENUMS(JUMP_LIGHTS, 2),
		RESETONRUN_LIGHT,
		STEPCLOCKS_LIGHT,
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
	const int cvMap[2][16] = {{0, 1, 2, 3, 4, 5, 6, 7, 0, 8, 9, 10, 11, 12, 13, 14},
							  {0, 8, 9 ,10, 11, 12, 13, 14, 0, 1, 2, 3, 4, 5, 6, 7}};// map each of the 16 steps of a sequence step to a CV knob index (0-14)

	// Need to save, with reset
	bool running;
	bool resetOnRun;
	bool quantize;// a.k.a. plank constant
	//bool symmetry;
	bool uncertainty;
	int stepIndexes[2];// position of electrons (sequencers)
	int states[2];// which clocks to use (0 = global, 1 = local, 2 = both)
	int ranges[2];// [0; 2], number of extra octaves to span each side of central octave (which is C4: 0 - 1V) 
	bool leap;
	
	
	// Need to save, no reset
	int panelTheme;
	
	// No need to save, with reset
	long clockIgnoreOnReset;
	float resetLight;
	bool rangeInc[2];// true when 1-3-5 increasing, false when 5-3-1 decreasing
	
	// No need to save, no reset
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTrigger;
	SchmittTrigger clocksTriggers[2];
	SchmittTrigger resetTrigger;
	SchmittTrigger stateTriggers[2];
	SchmittTrigger octTriggers[2];
	SchmittTrigger stateCVTriggers[2];
	SchmittTrigger leapTrigger;
	SchmittTrigger plankTrigger;
	SchmittTrigger uncertaintyTrigger;
	SchmittTrigger resetOnRunTrigger;
	SchmittTrigger stepClocksTrigger;
	PulseGenerator jumpPulses[2];
	float jumpLights[2];
	float stepClocksLight;

	
	inline float quantizeCV(float cv) {return roundf(cv * 12.0f) / 12.0f;}
	inline bool jumpRandom() {return (randomUniform() < (params[PROB_PARAM].value + inputs[PROB_INPUT].value / 10.0f));}// randomUniform is [0.0, 1.0), see include/util/common.hpp
	
	
	Ions() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		
		// No need to save, no reset		
		runningTrigger.reset();
		clockTrigger.reset();
		for (int i = 0; i < 2; i++) {
			clocksTriggers[i].reset();
			stateTriggers[i].reset();
			octTriggers[i].reset();
			stateCVTriggers[i].reset();
			jumpPulses[i].reset();
			jumpLights[i] = 0.0f;
		}
		stepClocksLight = 0.0f;
		resetTrigger.reset();
		leapTrigger.reset();
		plankTrigger.reset();
		uncertaintyTrigger.reset();
		resetOnRunTrigger.reset();
		stepClocksTrigger.reset();
		
		onReset();
	}

	
	// widgets are not yet created when module is created 
	// even if widgets not created yet, can use params[] and should handle 0.0f value since step may call 
	//   this before widget creation anyways
	// called from the main thread if by constructor, called by engine thread if right-click initialization
	//   when called by constructor, module is created before the first step() is called
	void onReset() override {
		// Need to save, with reset
		running = false;
		resetOnRun = false;
		quantize = true;
		//symmetry = false;
		uncertainty = false;
		for (int i = 0; i < 2; i++) {
			states[i] = 0;
			ranges[i] = 1;
		}
		leap = false;
		initRun(true, false);
		
		// No need to save, with reset
		for (int i = 0; i < 2; i++) {
			rangeInc[i] = true;
		}
	}

	
	// widgets randomized before onRandomize() is called
	// called by engine thread if right-click randomize
	void onRandomize() override {
		// Need to save, with reset
		running = false;
		resetOnRun = false;
		quantize = true;
		//symmetry = false;
		uncertainty = false;
		for (int i = 0; i < 2; i++) {
			states[i] = randomu32() % 3;
			ranges[i] = randomu32() % 3;
		}
		leap = (randomu32() % 2) > 0;
		initRun(true, true);
		
		// No need to save, with reset
		for (int i = 0; i < 2; i++) {
			rangeInc[i] = true;
		}
	}
	

	void initRun(bool hard, bool randomize) {// run button activated or run edge in run input jack
		if (hard) {
			if (randomize) {
				stepIndexes[0] = randomu32() % 16;
				stepIndexes[1] = randomu32() % 16;
			}
			else {
				stepIndexes[0] = 0;
				stepIndexes[1] = 0;
			}
		}
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
		resetLight = 0.0f;
	}
	
	// called by main thread
	json_t *toJson() override {
		json_t *rootJ = json_object();
		// Need to save (reset or not)

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// quantize
		json_object_set_new(rootJ, "quantize", json_boolean(quantize));
		
		// symmetry
		//json_object_set_new(rootJ, "symmetry", json_boolean(symmetry));
		
		// uncertainty
		json_object_set_new(rootJ, "uncertainty", json_boolean(uncertainty));
		
		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		// stepIndexes
		json_object_set_new(rootJ, "stepIndexes0", json_integer(stepIndexes[0]));
		json_object_set_new(rootJ, "stepIndexes1", json_integer(stepIndexes[1]));

		// states
		json_object_set_new(rootJ, "states0", json_integer(states[0]));
		json_object_set_new(rootJ, "states1", json_integer(states[1]));

		// ranges
		json_object_set_new(rootJ, "ranges0", json_integer(ranges[0]));
		json_object_set_new(rootJ, "ranges1", json_integer(ranges[1]));

		// leap
		json_object_set_new(rootJ, "leap", json_boolean(leap));

		return rootJ;
	}

	
	// widgets have their fromJson() called before this fromJson() is called
	// called by main thread
	void fromJson(json_t *rootJ) override {
		// Need to save (reset or not)

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		// quantize
		json_t *quantizeJ = json_object_get(rootJ, "quantize");
		if (quantizeJ)
			quantize = json_is_true(quantizeJ);

		// symmetry
		//json_t *symmetryJ = json_object_get(rootJ, "symmetry");
		//if (symmetryJ)
			//symmetry = json_is_true(symmetryJ);

		// uncertainty
		json_t *uncertaintyJ = json_object_get(rootJ, "uncertainty");
		if (uncertaintyJ)
			uncertainty = json_is_true(uncertaintyJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// stepIndexes
		json_t *stepIndexes0J = json_object_get(rootJ, "stepIndexes0");
		if (stepIndexes0J)
			stepIndexes[0] = json_integer_value(stepIndexes0J);
		json_t *stepIndexes1J = json_object_get(rootJ, "stepIndexes1");
		if (stepIndexes1J)
			stepIndexes[1] = json_integer_value(stepIndexes1J);

		// states
		json_t *states0J = json_object_get(rootJ, "states0");
		if (states0J)
			states[0] = json_integer_value(states0J);
		json_t *states1J = json_object_get(rootJ, "states1");
		if (states1J)
			states[1] = json_integer_value(states1J);

		// ranges
		json_t *ranges0J = json_object_get(rootJ, "ranges0");
		if (ranges0J)
			ranges[0] = json_integer_value(ranges0J);
		json_t *ranges1J = json_object_get(rootJ, "ranges1");
		if (ranges1J)
			ranges[1] = json_integer_value(ranges1J);

		// leap
		json_t *leapJ = json_object_get(rootJ, "leap");
		if (leapJ)
			leap = json_is_true(leapJ);

		// No need to save, with reset
		initRun(true, false);
		rangeInc[0] = true;
		rangeInc[1] = true;
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {	
		float sampleTime = engineGetSampleTime();
	
		//********** Buttons, knobs, switches and inputs **********
	
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUN_INPUT].value)) {
			running = !running;
			if (running)
				initRun(resetOnRun, false);
		}
		
		// Leap button
		if (leapTrigger.process(params[LEAP_PARAM].value)) {
			leap = !leap;
		}

		// Plank button (quatize)
		if (plankTrigger.process(params[PLANK_PARAM].value)) {
			quantize = !quantize;
		}

		// uncertainty button
		if (uncertaintyTrigger.process(params[uncertainty_PARAM].value)) {
			uncertainty = !uncertainty;
		}

		// Reset on Run button
		if (resetOnRunTrigger.process(params[RESETONRUN_PARAM].value)) {
			resetOnRun = !resetOnRun;
		}

		// State buttons and CV inputs
		for (int i = 0; i < 2; i++) {
			int stateTrig = stateTriggers[i].process(params[STATE_PARAMS + i].value);
			if (inputs[STATECV_INPUTS + i].active) {
				if (inputs[STATECV_INPUTS + i].value <= -1.0f)
					states[i] = 0;
				else if (inputs[STATECV_INPUTS + i].value < 1.0f)
					states[i] = 1;
				else 
					states[i] = 2;
			}
			else if (stateTrig) {
				states[i]++;
				if (states[i] >= 3)
					states[i] = 0;
			}
		}
		
		// Range buttons and CV inputs
		for (int i = 0; i < 2; i++) {
			int rangeTrig = octTriggers[i].process(params[OCT_PARAMS + i].value);
			if (inputs[OCTCV_INPUTS + i].active) {
				if (inputs[OCTCV_INPUTS + i].value <= -1.0f)
					ranges[i] = 0;
				else if (inputs[OCTCV_INPUTS + i].value < 1.0f)
					ranges[i] = 1;
				else 
					ranges[i] = 2;
			}
			else if (rangeTrig) {
				if (rangeInc[i]) {
					ranges[i]++;
					if (ranges[i] >= 3) {
						ranges[i] = 1;
						rangeInc[i] = false;
					}
				}
				else {
					ranges[i]--;
					if (ranges[i] < 0) {
						ranges[i] = 1;
						rangeInc[i] = true;
					}
				}
			}
		}

		

		//********** Clock and reset **********
		
		// Clocks
		bool globalClockTrig = clockTrigger.process(inputs[CLK_INPUT].value);
		bool stepClocksTrig = stepClocksTrigger.process(params[STEPCLOCKS_PARAM].value);
		for (int i = 0; i < 2; i++) {
			int jumpCount = 0;
			
			if (running && clockIgnoreOnReset == 0l) {	
				
				// Local clocks and uncertainty
				bool localClockTrig = clocksTriggers[i].process(inputs[CLK_INPUTS + i].value);
				localClockTrig &= (states[i] >= 1);
				if (localClockTrig) {
					if (uncertainty) {// local clock modified by uncertainty
						int numSteps = 8;
						int	prob = randomu32() % 1000;
						if (prob < 175)
							numSteps = 1;
						else if (prob < 330) // 175 + 155
							numSteps = 2;
						else if (prob < 475) // 175 + 155 + 145
							numSteps = 3;
						else if (prob < 610) // 175 + 155 + 145 + 135
							numSteps = 4;
						else if (prob < 725) // 175 + 155 + 145 + 135 + 115
							numSteps = 5;
						else if (prob < 830) // 175 + 155 + 145 + 135 + 115 + 105
							numSteps = 6;
						else if (prob < 925) // 175 + 155 + 145 + 135 + 115 + 105 + 95
							numSteps = 7;
						for (int n = 0; n < numSteps; n++)
							jumpCount += stepElectron(i, leap);
					}
					else 
						jumpCount += stepElectron(i, leap);// normal local clock
				}				
				
				// Global clock
				if (globalClockTrig && ((states[i] & 0x1) == 0) && !localClockTrig) {
					jumpCount += stepElectron(i, leap);
				}
				
			}
			
			// Magnetic clock (step clock)
			if (stepClocksTrig)
				jumpCount += stepElectron(i, leap);
			
			// Jump occurred feedback
			if ((jumpCount & 0x1) != 0) {
				jumpPulses[i].trigger(0.001f);
				jumpLights[i] = 1.0f;				
			}
		}
		//if (symmetry)
			//stepIndexes[1] = stepIndexes[0];
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			initRun(true, uncertainty);
			resetLight = 1.0f;
			clockTrigger.reset();
		}
		else
			resetLight -= (resetLight / lightLambda) * sampleTime;
		
		
		//********** Outputs and lights **********

		// Outputs
		for (int i = 0; i < 2; i++) {
			float knobVal = params[CV_PARAMS + cvMap[i][stepIndexes[i]]].value;
			float cv = 0.0f;
			int range = ranges[i];
			if (quantize) {
				cv = (knobVal * (float)(range * 2 + 1) - (float)range);
				cv = quantizeCV(cv);
			}
			else {
				int maxCV = (range == 0 ? 1 : (range * 5));// maxCV is [1, 5, 10]
				cv = knobVal * (float)(maxCV * 2) - (float)maxCV;
			}
			outputs[SEQ_OUTPUTS + i].value = cv;
			outputs[JUMP_OUTPUTS + i].value = jumpPulses[i].process((float)sampleTime);
		}
		
		// Blue and Yellow lights
		for (int i = 0; i < 16; i++) {
			lights[BLUE_LIGHTS + i].value = (stepIndexes[0] == i ? 1.0f : 0.0f);
			lights[YELLOW_LIGHTS + i].value = (stepIndexes[1] == i ? 1.0f : 0.0f);
		}
		
		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	
		
		// Run light
		lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;

		// State lights
		for (int i = 0; i < 2; i++) {
			lights[GLOBAL_LIGHTS + i].value = (states[i] & 0x1) == 0 ? 0.5f : 0.0f;
			lights[LOCAL_LIGHTS + i].value = states[i] >= 1 ? 0.5f : 0.0f;
		}
		
		// Leap, Plank, uncertainty and ResetOnRun lights
		lights[LEAP_LIGHT].value = leap ? 1.0f : 0.0f;
		lights[PLANK_LIGHT].value = quantize ? 1.0f : 0.0f;
		lights[uncertainty_LIGHT].value = uncertainty ? 1.0f : 0.0f;
		lights[RESETONRUN_LIGHT].value = resetOnRun ? 1.0f : 0.0f;
		
		// Range lights
		for (int i = 0; i < 3; i++) {
			lights[OCTA_LIGHTS + i].value = (i <= ranges[0] ? 1.0f : 0.0f);
			lights[OCTB_LIGHTS + i].value = (i <= ranges[1] ? 1.0f : 0.0f);
		}

		// Jump lights
		for (int i = 0; i < 2; i++) {
			lights[JUMP_LIGHTS + i].value = jumpLights[i];
			jumpLights[i] -= (jumpLights[i] / lightLambda) * sampleTime;
		}

		// Step clocks light
		if (stepClocksTrig)
			stepClocksLight = 1.0f;
		else
			stepClocksLight -= (stepClocksLight / lightLambda) * sampleTime;
		lights[STEPCLOCKS_LIGHT].value = stepClocksLight;
		
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}// step()
	
	
	int stepElectron(int i, bool leap) {
		int jumped = 0;
		int base = stepIndexes[i] & 0x8;// 0 or 8
		int step8 = stepIndexes[i] & 0x7;// 0 to 7
		if ( (step8 == 7 || leap) && jumpRandom() ) {
			jumped = 1;
			base = 8 - base;// change atom
		}
		step8++;
		if (step8 > 7)
			step8 = 0;
		stepIndexes[i] = base | step8;
		return jumped;
	}
};


struct IonsWidget : ModuleWidget {

	struct PanelThemeItem : MenuItem {
		Ions *module;
		int theme;
		void onAction(EventAction &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	Menu *createContextMenu() override {
		Menu *menu = ModuleWidget::createContextMenu();

		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Ions *module = dynamic_cast<Ions*>(this->module);
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
		//menu->addChild(darkItem);

		return menu;
	}	
	
	IonsWidget(Ions *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/IonsBG-01.svg")));
        //panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/IonsBG-02.svg")));// no dark pannel for now
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screws 
		// part of svg panel, no code required
		
		float colRulerCenter = box.size.x / 2.0f;
		static constexpr float rowRulerAtomA = 125.5;
		static constexpr float rowRulerAtomB = 251.5f;
		static constexpr float radius1 = 21.0f;
		static constexpr float offset1 = 14.0f;
		static constexpr float radius2 = 35.0f;
		static constexpr float offset2 = 25.0f;
		static constexpr float radius3 = 61.0f;
		static constexpr float offset3 = 43.0f;
		
		// Outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerAtomA), Port::OUTPUT, module, Ions::SEQ_OUTPUTS + 0, &module->panelTheme));		
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerAtomB), Port::OUTPUT, module, Ions::SEQ_OUTPUTS + 1, &module->panelTheme));		
		
		// CV knobs
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerAtomA + radius3 + 2.0f), module, Ions::CV_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerAtomA + offset3), module, Ions::CV_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + radius3, rowRulerAtomA), module, Ions::CV_PARAMS + 2, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerAtomA - offset3), module, Ions::CV_PARAMS + 3, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerAtomA - radius3), module, Ions::CV_PARAMS + 4, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerAtomA - offset3), module, Ions::CV_PARAMS + 5, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - radius3, rowRulerAtomA), module, Ions::CV_PARAMS + 6, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerAtomA + offset3), module, Ions::CV_PARAMS + 7, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		//
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerAtomB - offset3), module, Ions::CV_PARAMS + 8, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + radius3, rowRulerAtomB), module, Ions::CV_PARAMS + 9, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerAtomB + offset3), module, Ions::CV_PARAMS + 10, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerAtomB + radius3), module, Ions::CV_PARAMS + 11, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerAtomB + offset3), module, Ions::CV_PARAMS + 12, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - radius3, rowRulerAtomB), module, Ions::CV_PARAMS + 13, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerAtomB - offset3), module, Ions::CV_PARAMS + 14, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// Prob knob and CV inuput
		float probX = colRulerCenter + 2.0f * offset3;
		float probY = rowRulerAtomA + radius3 + 2.0f;
		addParam(createDynamicParam<GeoKnobLeft>(Vec(probX, probY), module, Ions::PROB_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(probX + 32.0f, probY), Port::INPUT, module, Ions::PROB_INPUT, &module->panelTheme));
		
		// Jump pulses
		addOutput(createDynamicPort<GeoPort>(Vec(probX + 18.0f, probY - 37.0f), Port::OUTPUT, module, Ions::JUMP_OUTPUTS + 0, &module->panelTheme));		
		addOutput(createDynamicPort<GeoPort>(Vec(probX + 18.0f, probY + 37.0f), Port::OUTPUT, module, Ions::JUMP_OUTPUTS + 1, &module->panelTheme));		
		// Jump lights
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(probX, probY - 46.0f), module, Ions::JUMP_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(probX, probY + 46.0f), module, Ions::JUMP_LIGHTS + 1));
		
		// Leap light and button
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - 86.5f, 62.5f), module, Ions::LEAP_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - 77.5f, 50.5f), module, Ions::LEAP_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	

		// Plank light and button
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + 86.5f, 62.5f), module, Ions::PLANK_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + 77.5f, 50.5f), module, Ions::PLANK_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	

		// Octave buttons and lights
		float octX = colRulerCenter + 107.0f;
		float octOffsetY = 10.0f;
		float octYA = rowRulerAtomA - octOffsetY;
		float octYB = rowRulerAtomB + octOffsetY;
		// top:
		addParam(createDynamicParam<GeoPushButton>(Vec(octX, octYA), module, Ions::OCT_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(octX - 15.0f, octYA + 2.5f), module, Ions::OCTA_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(octX - 12.0f, octYA - 8.0f), module, Ions::OCTA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(octX - 10.0f, octYA + 11.5f), module, Ions::OCTA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(octX - 3.0f, octYA - 13.5f), module, Ions::OCTA_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(octX + 0.0f, octYA + 15.0f), module, Ions::OCTA_LIGHTS + 2));		
		// bottom:
		addParam(createDynamicParam<GeoPushButton>(Vec(octX, octYB), module, Ions::OCT_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(octX - 15.0f, octYB - 2.5f), module, Ions::OCTB_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(octX - 12.0f, octYB + 8.0f), module, Ions::OCTB_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(octX - 10.0f, octYB - 11.5f), module, Ions::OCTB_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(octX - 3.0f, octYB + 13.5f), module, Ions::OCTB_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(octX + 0.0f, octYB - 15.0f), module, Ions::OCTB_LIGHTS + 2));
		// Oct CV inputs
		addInput(createDynamicPort<GeoPort>(Vec(octX - 9.0f, octYA - 34.5), Port::INPUT, module, Ions::OCTCV_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(octX - 9.0f, octYB + 34.5), Port::INPUT, module, Ions::OCTCV_INPUTS + 1, &module->panelTheme));
		
		// Blue electron lights
		// top blue
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerAtomA + radius2), module, Ions::BLUE_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offset2, rowRulerAtomA + offset2), module, Ions::BLUE_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + radius2, rowRulerAtomA), module, Ions::BLUE_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offset2, rowRulerAtomA - offset2), module, Ions::BLUE_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerAtomA - radius2), module, Ions::BLUE_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offset2, rowRulerAtomA - offset2), module, Ions::BLUE_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - radius2, rowRulerAtomA), module, Ions::BLUE_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offset2, rowRulerAtomA + offset2), module, Ions::BLUE_LIGHTS + 7));
		// bottom blue
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerAtomB - radius1), module, Ions::BLUE_LIGHTS + 8));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offset1, rowRulerAtomB - offset1), module, Ions::BLUE_LIGHTS + 9));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + radius1, rowRulerAtomB), module, Ions::BLUE_LIGHTS + 10));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offset1, rowRulerAtomB + offset1), module, Ions::BLUE_LIGHTS + 11));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerAtomB + radius1), module, Ions::BLUE_LIGHTS + 12));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offset1, rowRulerAtomB + offset1), module, Ions::BLUE_LIGHTS + 13));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - radius1, rowRulerAtomB), module, Ions::BLUE_LIGHTS + 14));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offset1, rowRulerAtomB - offset1), module, Ions::BLUE_LIGHTS + 15));
		
		// Yellow electron lights
		// bottom yellow
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter, rowRulerAtomB - radius2), module, Ions::YELLOW_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + offset2, rowRulerAtomB - offset2), module, Ions::YELLOW_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + radius2, rowRulerAtomB), module, Ions::YELLOW_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + offset2, rowRulerAtomB + offset2), module, Ions::YELLOW_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter, rowRulerAtomB + radius2), module, Ions::YELLOW_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - offset2, rowRulerAtomB + offset2), module, Ions::YELLOW_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - radius2, rowRulerAtomB), module, Ions::YELLOW_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - offset2, rowRulerAtomB - offset2), module, Ions::YELLOW_LIGHTS + 7));		
		// top yellow
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter, rowRulerAtomA + radius1), module, Ions::YELLOW_LIGHTS + 8));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + offset1, rowRulerAtomA + offset1), module, Ions::YELLOW_LIGHTS + 9));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + radius1, rowRulerAtomA), module, Ions::YELLOW_LIGHTS + 10));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + offset1, rowRulerAtomA - offset1), module, Ions::YELLOW_LIGHTS + 11));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter, rowRulerAtomA - radius1), module, Ions::YELLOW_LIGHTS + 12));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - offset1, rowRulerAtomA - offset1), module, Ions::YELLOW_LIGHTS + 13));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - radius1, rowRulerAtomA), module, Ions::YELLOW_LIGHTS + 14));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - offset1, rowRulerAtomA + offset1), module, Ions::YELLOW_LIGHTS + 15));

		// Run jack, light and button
		static constexpr float rowRulerRunJack = 344.5f;
		static constexpr float offsetRunJackX = 119.5f;
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetRunJackX, rowRulerRunJack), Port::INPUT, module, Ions::RUN_INPUT, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetRunJackX + 18.0f, rowRulerRunJack), module, Ions::RUN_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetRunJackX + 33.0f, rowRulerRunJack), module, Ions::RUN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		
		// Reset jack, light and button
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetRunJackX, rowRulerRunJack), Port::INPUT, module, Ions::RESET_INPUT, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetRunJackX - 18.0f, rowRulerRunJack), module, Ions::RESET_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetRunJackX - 33.0f, rowRulerRunJack), module, Ions::RESET_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
	
		static constexpr float offsetMagneticButton = 42.5f;
		// Magnetic clock (step clocks)
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetMagneticButton - 15.0f, rowRulerRunJack), module, Ions::STEPCLOCKS_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetMagneticButton, rowRulerRunJack), module, Ions::STEPCLOCKS_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));			
		// Reset on Run light and button
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetMagneticButton + 15.0f, rowRulerRunJack), module, Ions::RESETONRUN_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetMagneticButton, rowRulerRunJack), module, Ions::RESETONRUN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	

		
		// Globak clock
		float gclkX = colRulerCenter - 2.0f * offset3;
		float gclkY = rowRulerAtomA + radius3 + 2.0f;
		addInput(createDynamicPort<GeoPort>(Vec(gclkX, gclkY), Port::INPUT, module, Ions::CLK_INPUT, &module->panelTheme));
		// global lights
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(gclkX - 12.0f, gclkY - 20.0f), module, Ions::GLOBAL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(gclkX - 12.0f, gclkY + 20.0f), module, Ions::GLOBAL_LIGHTS + 1));
		// state buttons
		addParam(createDynamicParam<GeoPushButton>(Vec(gclkX - 17.0f, gclkY - 34.0f), module, Ions::STATE_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addParam(createDynamicParam<GeoPushButton>(Vec(gclkX - 17.0f, gclkY + 34.0f), module, Ions::STATE_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		// local lights
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(gclkX - 20.0f, gclkY - 48.5f), module, Ions::LOCAL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(gclkX - 20.0f, gclkY + 48.5f), module, Ions::LOCAL_LIGHTS + 1));
		// local inputs
		addInput(createDynamicPort<GeoPort>(Vec(gclkX - 21.0f, gclkY - 72.0f), Port::INPUT, module, Ions::CLK_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(gclkX - 21.0f, gclkY + 72.0f), Port::INPUT, module, Ions::CLK_INPUTS + 1, &module->panelTheme));
		// state inputs
		addInput(createDynamicPort<GeoPort>(Vec(gclkX - 11.0f, gclkY - 107.0f), Port::INPUT, module, Ions::STATECV_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(gclkX - 11.0f, gclkY + 107.0f), Port::INPUT, module, Ions::STATECV_INPUTS + 1, &module->panelTheme));
		
		// uncertainty light and button
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(gclkX - 20.0f, gclkY), module, Ions::uncertainty_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(gclkX - 34.0f, gclkY), module, Ions::uncertainty_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	

	}
};

Model *modelIons = Model::create<Ions, IonsWidget>("Geodesics", "Ions", "Ions", SEQUENCER_TAG);

/*CHANGE LOG

0.6.1:
Ions reloaded (many changes)

0.6.0:
created

*/