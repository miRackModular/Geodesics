//***********************************************************************************************
//Atomic Dual Sequencer module for VCV Rack by Pierre Collard and Marc Boulé
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
		NUM_PARAMS
	};
	enum InputIds {
		CLK_INPUT,
		ENUMS(CLK_INPUTS, 2),
		RUN_INPUT,
		RESET_INPUT,
		PROB_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(SEQ_OUTPUTS, 2),
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
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)

	// Need to save, with reset
	bool running;
	int stepIndexBlue;
	int stepIndexYellow;
	int stateBlue;
	int stateYellow;
	bool leap;
	
	
	// Need to save, no reset
	int panelTheme;
	bool resetOnRun;
	
	// No need to save, with reset
	long clockIgnoreOnReset;
	float resetLight;
	
	// No need to save, no reset
	SchmittTrigger runningTrigger;
	SchmittTrigger clockTrigger;
	SchmittTrigger clocksTriggers[2];
	SchmittTrigger resetTrigger;
	SchmittTrigger stateTriggers[2];
	SchmittTrigger leapTrigger;

	
	Ions() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		resetOnRun = false;
		
		// No need to save, no reset		
		runningTrigger.reset();
		clockTrigger.reset();
		for (int i = 0; i < 2; i++) {
			clocksTriggers[i].reset();
			stateTriggers[i].reset();
		}
		resetTrigger.reset();
		leapTrigger.reset();
		
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
		stateBlue = 0;
		stateYellow = 0;
		leap = false;
		initRun(true);
		
		// No need to save, with reset
		// none
	}

	
	// widgets randomized before onRandomize() is called
	// called by engine thread if right-click randomize
	void onRandomize() override {
		// Need to save, with reset
		running = false;
		stateBlue = randomu32() % 3;
		stateYellow = randomu32() % 3;
		leap = (randomu32() % 2) > 0;
		initRun(true);
		stepIndexBlue = randomu32() % 16;
		stepIndexYellow = randomu32() % 16;
		
		// No need to save, with reset
		// none
	}
	

	void initRun(bool hard) {// run button activated or run edge in run input jack
		if (hard) {
			stepIndexBlue = 0;
			stepIndexYellow = 0;
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
		
		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		// stepIndexBlue
		json_object_set_new(rootJ, "stepIndexBlue", json_integer(stepIndexBlue));

		// stepIndexYellow
		json_object_set_new(rootJ, "stepIndexYellow", json_integer(stepIndexYellow));

		// stateBlue
		json_object_set_new(rootJ, "stateBlue", json_integer(stateBlue));

		// stateYellow
		json_object_set_new(rootJ, "stateYellow", json_integer(stateYellow));

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

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// stepIndexBlue
		json_t *stepIndexBlueJ = json_object_get(rootJ, "stepIndexBlue");
		if (stepIndexBlueJ)
			stepIndexBlue = json_integer_value(stepIndexBlueJ);

		// stateBlue
		json_t *stateBlueJ = json_object_get(rootJ, "stateBlue");
		if (stateBlueJ)
			stateBlue = json_integer_value(stateBlueJ);

		// stepIndexYellow
		json_t *stepIndexYellowJ = json_object_get(rootJ, "stepIndexYellow");
		if (stepIndexYellowJ)
			stepIndexYellow = json_integer_value(stepIndexYellowJ);

		// stateYellow
		json_t *stateYellowJ = json_object_get(rootJ, "stateYellow");
		if (stateYellowJ)
			stateYellow = json_integer_value(stateYellowJ);

		// leap
		json_t *leapJ = json_object_get(rootJ, "leap");
		if (leapJ)
			leap = json_is_true(leapJ);

		// No need to save, with reset
		initRun(true);
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {	


		//********** Buttons, knobs, switches and inputs **********
	
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUN_INPUT].value)) {
			running = !running;
			if (running)
				initRun(resetOnRun);
		}
		
		// Leap button
		if (leapTrigger.process(params[LEAP_PARAM].value)) {
			leap = !leap;
		}

		// State buttons and CV inputs
		// Blue state
		if (stateTriggers[0].process(params[STATE_PARAMS + 0].value + inputs[CLK_INPUTS + 0].value)) {
			stateBlue ++;
			if (stateBlue >= 3)
				stateBlue = 0;
		}
		// Yellow state
		if (stateTriggers[1].process(params[STATE_PARAMS + 1].value + inputs[CLK_INPUTS + 1].value)) {
			stateYellow ++;
			if (stateYellow >= 3)
				stateYellow = 0;
		}
		

		//********** Clock and reset **********
		
		// Clock
		bool globalClockTrig = clockTrigger.process(inputs[CLK_INPUT].value);
		bool blueClockTrig = clocksTriggers[0].process(inputs[CLK_INPUTS + 0].value);
		bool yellowClockTrig = clocksTriggers[1].process(inputs[CLK_INPUTS + 1].value);
		if (globalClockTrig || (blueClockTrig && stateBlue >= 1)) {
			if (running && clockIgnoreOnReset == 0l) {
				/*int base = stepIndexBlue & 0x8; // 0 or 8
				int step8 = stepIndexBlue & 0x7; // 0 to 7
				step8++;
				
				if ( (step8 == 8 || leap) && inputs[PROB_PARAM].value > 0.5f) {
					base = 8 - base;// change atom
				}*/
				stepIndexBlue++;
				if (stepIndexBlue >= 16)
					stepIndexBlue = 0;
			}
		}
		if (globalClockTrig || (yellowClockTrig && stateYellow >= 1)) {
			if (running && clockIgnoreOnReset == 0l) {
				stepIndexYellow++;
				if (stepIndexYellow >= 16)
					stepIndexYellow = 0;
			}
		}
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			initRun(true);// must be after sequence reset
			resetLight = 1.0f;
			clockTrigger.reset();
		}
		else
			resetLight -= (resetLight / lightLambda) * engineGetSampleTime();
		
		
		//********** Outputs and lights **********

		// Blue and Yellow lights
		for (int i = 0; i < 16; i++) {
			lights[BLUE_LIGHTS + i].value = (stepIndexBlue == i ? 1.0f : 0.0f);
			lights[YELLOW_LIGHTS + i].value = (stepIndexYellow == i ? 1.0f : 0.0f);
		}
		
		// Reset light
		lights[RESET_LIGHT].value =	resetLight;	
		
		// Run light
		lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;

		// State lights
		lights[GLOBAL_LIGHTS + 0].value = (stateBlue & 0x1) == 0 ? 1.0f : 0.0f;
		lights[GLOBAL_LIGHTS + 1].value = (stateYellow & 0x1) == 0 ? 1.0f : 0.0f;
		lights[LOCAL_LIGHTS + 0].value = stateBlue >= 1 ? 1.0f : 0.0f;
		lights[LOCAL_LIGHTS + 1].value = stateYellow >= 1 ? 1.0f : 0.0f;
		
		// Leap light
		lights[LEAP_LIGHT].value = leap ? 1.0f : 0.0f;
		
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}// step()
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
	struct ResetOnRunItem : MenuItem {
		Ions *module;
		void onAction(EventAction &e) override {
			module->resetOnRun = !module->resetOnRun;
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

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);
		
		ResetOnRunItem *rorItem = MenuItem::create<ResetOnRunItem>("Reset on Run", CHECKMARK(module->resetOnRun));
		rorItem->module = module;
		menu->addChild(rorItem);

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
		static constexpr float rowRulerAtomA = 115.12;
		static constexpr float rowRulerAtomB = 241.12f;
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
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerAtomA + radius3 + 2.0f), module, Ions::CV_PARAMS + 0, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerAtomA + offset3), module, Ions::CV_PARAMS + 1, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + radius3, rowRulerAtomA), module, Ions::CV_PARAMS + 2, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerAtomA - offset3), module, Ions::CV_PARAMS + 3, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerAtomA - radius3), module, Ions::CV_PARAMS + 4, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerAtomA - offset3), module, Ions::CV_PARAMS + 5, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - radius3, rowRulerAtomA), module, Ions::CV_PARAMS + 6, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerAtomA + offset3), module, Ions::CV_PARAMS + 7, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		//
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerAtomB - offset3), module, Ions::CV_PARAMS + 8, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + radius3, rowRulerAtomB), module, Ions::CV_PARAMS + 9, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerAtomB + offset3), module, Ions::CV_PARAMS + 10, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerAtomB + radius3), module, Ions::CV_PARAMS + 11, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerAtomB + offset3), module, Ions::CV_PARAMS + 12, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - radius3, rowRulerAtomB), module, Ions::CV_PARAMS + 13, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerAtomB - offset3), module, Ions::CV_PARAMS + 14, -1.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// Prob knob and CV inuput
		float probX = colRulerCenter + 2.0f * offset3;
		float probY = rowRulerAtomA + radius3 + 2.0f;
		addParam(createDynamicParam<GeoKnob>(Vec(probX, probY), module, Ions::PROB_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(probX + 15.0f, probY + 30.0f), Port::INPUT, module, Ions::PROB_INPUT, &module->panelTheme));
		
		// Leap light and button
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(probX + 12.0f, probY - 27.0f), module, Ions::LEAP_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(probX + 17.0f, probY - 40.0f), module, Ions::LEAP_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	

		// Octave buttons and lights
		float octX = colRulerCenter + 98.0f;
		float octOffsetY = 41.0f;
		float octYA = rowRulerAtomA - octOffsetY;
		float octYB = rowRulerAtomB + octOffsetY;
		// top:
		addParam(createDynamicParam<GeoPushButton>(Vec(octX, octYA), module, Ions::OCT_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(octX - 13.0f, octYA + 7.0f), module, Ions::OCTA_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(octX - 14.0f, octYA - 4.0f), module, Ions::OCTA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(octX - 6.0f, octYA + 14.0f), module, Ions::OCTA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(octX - 7.0f, octYA - 12.0f), module, Ions::OCTA_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(octX + 5.0f, octYA + 14.0f), module, Ions::OCTA_LIGHTS + 2));		
		// bottom:
		addParam(createDynamicParam<GeoPushButton>(Vec(octX, octYB), module, Ions::OCT_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(octX - 13.0f, octYB - 7.0f), module, Ions::OCTA_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(octX - 14.0f, octYB + 4.0f), module, Ions::OCTA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(octX - 6.0f, octYB - 14.0f), module, Ions::OCTA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(octX - 7.0f, octYB + 12.0f), module, Ions::OCTA_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(octX + 5.0f, octYB - 14.0f), module, Ions::OCTA_LIGHTS + 2));
		
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
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter, rowRulerAtomB - radius2), module, Ions::BLUE_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + offset2, rowRulerAtomB - offset2), module, Ions::BLUE_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + radius2, rowRulerAtomB), module, Ions::BLUE_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + offset2, rowRulerAtomB + offset2), module, Ions::BLUE_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter, rowRulerAtomB + radius2), module, Ions::BLUE_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - offset2, rowRulerAtomB + offset2), module, Ions::BLUE_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - radius2, rowRulerAtomB), module, Ions::BLUE_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - offset2, rowRulerAtomB - offset2), module, Ions::BLUE_LIGHTS + 7));		
		// top yellow
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter, rowRulerAtomA + radius1), module, Ions::BLUE_LIGHTS + 8));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + offset1, rowRulerAtomA + offset1), module, Ions::BLUE_LIGHTS + 9));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + radius1, rowRulerAtomA), module, Ions::BLUE_LIGHTS + 10));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + offset1, rowRulerAtomA - offset1), module, Ions::BLUE_LIGHTS + 11));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter, rowRulerAtomA - radius1), module, Ions::BLUE_LIGHTS + 12));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - offset1, rowRulerAtomA - offset1), module, Ions::BLUE_LIGHTS + 13));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - radius1, rowRulerAtomA), module, Ions::BLUE_LIGHTS + 14));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - offset1, rowRulerAtomA + offset1), module, Ions::BLUE_LIGHTS + 15));

		// Run jack, light and button
		static constexpr float rowRulerRunJack = 343.12f;
		static constexpr float offsetRunJackX = 31.0f;
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetRunJackX, rowRulerRunJack), Port::INPUT, module, Ions::RUN_INPUT, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetRunJackX - 18.0f, rowRulerRunJack - 7.0f), module, Ions::RUN_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetRunJackX - 30.0f, rowRulerRunJack - 15.0f), module, Ions::RUN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		
		// Reset jack, light and button
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetRunJackX, rowRulerRunJack), Port::INPUT, module, Ions::RESET_INPUT, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetRunJackX + 18.0f, rowRulerRunJack - 7.0f), module, Ions::RESET_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetRunJackX + 30.0f, rowRulerRunJack - 15.0f), module, Ions::RESET_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
	
		// Globak clock
		float gclkX = colRulerCenter - 2.0f * offset3;
		float gclkY = rowRulerAtomA + radius3 + 2.0f;
		addInput(createDynamicPort<GeoPort>(Vec(gclkX, gclkY), Port::INPUT, module, Ions::CLK_INPUT, &module->panelTheme));
		// global lights
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(gclkX - 9.0f, gclkY - 16.0f), module, Ions::GLOBAL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(gclkX - 9.0f, gclkY + 16.0f), module, Ions::GLOBAL_LIGHTS + 1));
		// state buttons
		addParam(createDynamicParam<GeoPushButton>(Vec(gclkX - 15.0f, gclkY - 30.0f), module, Ions::STATE_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		addParam(createDynamicParam<GeoPushButton>(Vec(gclkX - 15.0f, gclkY + 30.0f), module, Ions::STATE_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		// local lights
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(gclkX - 18.0f, gclkY - 44.0f), module, Ions::LOCAL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(gclkX - 18.0f, gclkY + 44.0f), module, Ions::LOCAL_LIGHTS + 1));
		// local inputs
		addInput(createDynamicPort<GeoPort>(Vec(gclkX - 20.0f, gclkY - 63.0f), Port::INPUT, module, Ions::CLK_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(gclkX - 20.0f, gclkY + 63.0f), Port::INPUT, module, Ions::CLK_INPUTS + 1, &module->panelTheme));
	}
};

Model *modelIons = Model::create<Ions, IonsWidget>("Geodesics", "Ions", "Ions", SEQUENCER_TAG);

/*CHANGE LOG

0.6.0:
created

*/


/*

Tout fonctionne comme dans le manuel, il y a juste quelques détails qui changent :
Le state clock a 3 états : global, local, et global + local ( 2 clock a la fois comme dans branes)
Le run et reset expériment ont leur bouton et cv propres, ils contrôlent les deux séquenceur à la fois.
Pour « energy » qui est en fait le range de la séquence, il n’est pas liés au knob, mais au séquences, la séquence bleu peut avoir un range différent que la séquence jaune, même en passant par les mêmes knob, je ne sais pas si c’est possible
 
energy à 3 états
o o O o o : range un octave
o O O O o : range trois octaves
O O O O O : range cinq octaves
 
J’ai mis ci-joint une version des leds en couleur pour que tu puisse t’y retrouve
*/