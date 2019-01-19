//***********************************************************************************************
//Thermodynamic Evolving Sequencer module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"


struct Entropy : Module {
	enum ParamIds {
		RUN_PARAM,
		STEPCLOCK_PARAM,// magnetic clock
		RESET_PARAM,
		RESETONRUN_PARAM,
		LENGTH_PARAM,
		ENUMS(CV_PARAMS, 16),// first 8 are blue, last 8 are yellow
		ENUMS(PROB_PARAMS, 8),// prob knobs
		ENUMS(OCT_PARAMS, 2),// energy
		ENUMS(QUANTIZE_PARAMS, 2),// plank
		ENUMS(STATE_PARAMS, 2),// state switch
		ENUMS(FIXEDCV_PARAMS, 2),
		ENUMS(EXTSIG_PARAMS, 2),
		ENUMS(RANDOM_PARAMS, 2),
		GPROB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CERTAIN_CLK_INPUT,
		UNCERTAIN_CLK_INPUT,
		LENGTH_INPUT,
		RUN_INPUT,
		RESET_INPUT,
		ENUMS(STATE_INPUTS, 2),
		ENUMS(OCT_INPUTS, 2),
		ENUMS(EXTSIG_INPUTS, 2),
		ENUMS(QUANTIZE_INPUTS, 2),
		GPROB_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CV_OUTPUT,// main output
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_LIGHTS, 16),// first 8 are blue, last 8 are yellow
		ENUMS(CV_LIGHT, 2),// main output (room for blue/yellow)
		RUN_LIGHT,
		STEPCLOCK_LIGHT,
		RESET_LIGHT,
		RESETONRUN_LIGHT,
		ENUMS(LENGTH_LIGHTS, 8),// all off when len = 8, north-west turns on when len = 7, north-west and west on when len = 6, etc
		ENUMS(STATE_LIGHTS, 2),
		ADD_LIGHT,
		ENUMS(QUANTIZE_LIGHTS, 2),
		ENUMS(ENERGY_LIGHTS, 6),// first 3 are blue, last 3 are yellow (symetrical so only 3 instead of 5 declared)
		ENUMS(FIXEDCV_LIGHTS, 2),
		ENUMS(EXTSIG_LIGHTS, 2),
		ENUMS(RANDOM_LIGHTS, 2),
		OUTPUT_LIGHT,
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float clockIgnoreOnResetDuration = 0.001f;// disable clock on powerup and reset for 1 ms (so that the first step plays)
							  
	// Need to save, with reset
	int panelTheme = 0;
	bool running;
	bool resetOnRun;
	int length;
	int quantize;// a.k.a. plank constant, bit0 = blue, bit1 = yellow
	
	
	// No need to save
	long clockIgnoreOnReset;
	float resetLight;
	unsigned int lightRefreshCounter = 0;
	int stepIndex;
	bool pipeBlue[8];
	SchmittTrigger runningTrigger;
	SchmittTrigger plankTriggers[2];
	SchmittTrigger lengthTrigger;
	SchmittTrigger certainClockTrigger;
	SchmittTrigger uncertainClockTrigger;
	SchmittTrigger stepClockTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger resetOnRunTrigger;
	float stepClockLight = 0.0f;
	
	inline float quantizeCV(float cv) {return roundf(cv * 12.0f) / 12.0f;}
	inline void updatePipeBlue(int step) {pipeBlue[step] = params[PROB_PARAMS + step].value > randomUniform();}
	
	
	Entropy() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		onReset();
	}

	
	void onReset() override {
		running = false;
		resetOnRun = false;
		length = 8;
		quantize = 3;
		initRun(true, false);
	}

	
	void onRandomize() override {
		length = (randomu32() & 0x7) + 1;
		quantize = randomu32() & 0x3;
		initRun(true, true);
	}
	

	void initRun(bool hard, bool randomize) {// run button activated or run edge in run input jack
		if (hard) {
			if (randomize) {
				stepIndex = randomu32() % length;
			}
			else {
				stepIndex = 0;
			}
			for (int i = 0; i < 8; i++)
				updatePipeBlue(i);
		}
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * engineGetSampleRate());
		resetLight = 0.0f;
	}
	

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));

		// length
		json_object_set_new(rootJ, "length", json_integer(length));

		// quantize
		json_object_set_new(rootJ, "quantize", json_integer(quantize));

		return rootJ;
	}

	
	void fromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		// length
		json_t *lengthJ = json_object_get(rootJ, "length");
		if (lengthJ)
			length = json_integer_value(lengthJ);

		// quantize
		json_t *quantizeJ = json_object_get(rootJ, "quantize");
		if (quantizeJ)
			quantize = json_integer_value(quantizeJ);

		initRun(true, false);
	}

	
	void step() override {	
		float sampleTime = engineGetSampleTime();
	
		//********** Buttons, knobs, switches and inputs **********
	
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].value + inputs[RUN_INPUT].value)) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			if (running)
				initRun(resetOnRun, false);
		}
		
		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {

			// Length button
			if (lengthTrigger.process(params[LENGTH_PARAM].value)) {
				if (length > 1) length--;
				else length = 8;
			}

			// Plank buttons (quantize)
			if (plankTriggers[0].process(params[QUANTIZE_PARAMS + 0].value))
				quantize ^= 0x1;
			if (plankTriggers[1].process(params[QUANTIZE_PARAMS + 1].value))
				quantize ^= 0x2;

			
			// Reset on Run button
			if (resetOnRunTrigger.process(params[RESETONRUN_PARAM].value)) {
				resetOnRun = !resetOnRun;
			}		
		
		}// userInputs refresh
		

		//********** Clock and reset **********
		
		// Clocks
		bool certainClockTrig = certainClockTrigger.process(inputs[CERTAIN_CLK_INPUT].value);
		// bool uncertainClockTrig = uncertainClockTrigger.process(inputs[UNCERTAIN_CLK_INPUT].value);
		bool stepClockTrig = stepClockTrigger.process(params[STEPCLOCK_PARAM].value);
		if (running && clockIgnoreOnReset == 0l) {
			if (certainClockTrig) {
				if (++stepIndex >= length) stepIndex = 0;
				updatePipeBlue(stepIndex);
			}
		}				
		// Magnetic clock (step clock)
		if (stepClockTrig) {
			if (++stepIndex >= length) stepIndex = 0;
			updatePipeBlue(stepIndex);
		}
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].value + params[RESET_PARAM].value)) {
			initRun(true, false);
			resetLight = 1.0f;
			certainClockTrigger.reset();
			uncertainClockTrigger.reset();
		}
		
		
		//********** Outputs and lights **********

		// Output
		outputs[CV_OUTPUT].value = 0.0f;
		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;

			// Reset light
			lights[RESET_LIGHT].value =	resetLight;	
			resetLight -= (resetLight / lightLambda) * sampleTime * displayRefreshStepSkips;	
			
			// Run light
			lights[RUN_LIGHT].value = running ? 1.0f : 0.0f;
			lights[RESETONRUN_LIGHT].value = resetOnRun ? 1.0f : 0.0f;
			
			// Length lights
			for (int i = 0; i < 8; i++)
				lights[LENGTH_LIGHTS + i].value = (i < length ? 0.0f : 1.0f);
			
			// Plank
			lights[QUANTIZE_LIGHTS + 0].value = (quantize & 0x1) ? 1.0f : 0.0f;// Blue
			lights[QUANTIZE_LIGHTS + 1].value = (quantize & 0x2) ? 1.0f : 0.0f;// Yellow

			// step and main output lights
			lights[CV_LIGHT + 0].value = pipeBlue[stepIndex] ? 1.0f : 0.0f;
			lights[CV_LIGHT + 1].value = (!pipeBlue[stepIndex]) ? 1.0f : 0.0f;
			for (int i = 0; i < 8; i++) {
				lights[STEP_LIGHTS + i].value = (pipeBlue[i] && stepIndex == i) ? 1.0f : 0.0f;
				lights[STEP_LIGHTS + 8 + i].value = (!pipeBlue[i] && stepIndex == i) ? 1.0f : 0.0f;
			}
				
			// Step clocks light
			if (stepClockTrig)
				stepClockLight = 1.0f;
			else
				stepClockLight -= (stepClockLight / lightLambda) * sampleTime * displayRefreshStepSkips;
			lights[STEPCLOCK_LIGHT].value = stepClockLight;

		}// lightRefreshCounter
		
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}// step()
	
};


struct EntropyWidget : ModuleWidget {

	struct PanelThemeItem : MenuItem {
		Entropy *module;
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

		Entropy *module = dynamic_cast<Entropy*>(this->module);
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

		return menu;
	}	
	
	EntropyWidget(Entropy *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/WhiteLight/Entropy-WL.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/DarkMatter/Entropy-DM.svg")));
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screws 
		// part of svg panel, no code required
		
		static constexpr float colRulerCenter = 157.0f;//box.size.x / 2.0f;
		static constexpr float rowRulerOutput = 380.0f - 155.5f;
		static constexpr float radius1 = 50.0f;
		static constexpr float offset1 = 35.5f;
		static constexpr float radius3 = 105.0f;
		static constexpr float offset3 = 74.5f;
		static constexpr float offset2b = 74.5f;// big
		static constexpr float offset2s = 27.5f;// small
		
		
		// CV out and light 
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerOutput), Port::OUTPUT, module, Entropy::CV_OUTPUT, &module->panelTheme));		
		addChild(createLightCentered<SmallLight<GeoBlueYellowLight>>(Vec(colRulerCenter, rowRulerOutput - 21.5f), module, Entropy::CV_LIGHT));
		
		// Blue CV knobs
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerOutput - radius1), module, Entropy::CV_PARAMS + 0, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset1, rowRulerOutput - offset1), module, Entropy::CV_PARAMS + 1, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + radius1, rowRulerOutput), module, Entropy::CV_PARAMS + 2, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset1, rowRulerOutput + offset1), module, Entropy::CV_PARAMS + 3, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerOutput + radius1), module, Entropy::CV_PARAMS + 4, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset1, rowRulerOutput + offset1), module, Entropy::CV_PARAMS + 5, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - radius1, rowRulerOutput), module, Entropy::CV_PARAMS + 6, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset1, rowRulerOutput - offset1), module, Entropy::CV_PARAMS + 7, 0.0f, 1.0f, 0.5f, &module->panelTheme));

		// Yellow CV knobs
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerOutput - radius3), module, Entropy::CV_PARAMS + 8 + 0, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerOutput - offset3), module, Entropy::CV_PARAMS + 8 + 1, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + radius3, rowRulerOutput), module, Entropy::CV_PARAMS + 8 + 2, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offset3, rowRulerOutput + offset3), module, Entropy::CV_PARAMS + 8 + 3, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerOutput + radius3), module, Entropy::CV_PARAMS + 8 + 4, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerOutput + offset3), module, Entropy::CV_PARAMS + 8 + 5, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - radius3, rowRulerOutput), module, Entropy::CV_PARAMS + 8 + 6, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset3, rowRulerOutput - offset3), module, Entropy::CV_PARAMS + 8 + 7, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		
		// Prob CV knobs
		addParam(createDynamicParam<GeoKnobRight>(Vec(colRulerCenter + offset2s, rowRulerOutput - offset2b - 3), module, Entropy::PROB_PARAMS + 0, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnobBotRight>(Vec(colRulerCenter + offset2b, rowRulerOutput - offset2s - 8), module, Entropy::PROB_PARAMS + 1, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnobBottom>(Vec(colRulerCenter + offset2b + 3, rowRulerOutput + offset2s), module, Entropy::PROB_PARAMS + 2, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnobBotLeft>(Vec(colRulerCenter + offset2s + 8, rowRulerOutput + offset2b), module, Entropy::PROB_PARAMS + 3, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnobLeft>(Vec(colRulerCenter - offset2s, rowRulerOutput + offset2b + 3), module, Entropy::PROB_PARAMS + 4, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnobTopLeft>(Vec(colRulerCenter - offset2b, rowRulerOutput + offset2s + 8), module, Entropy::PROB_PARAMS + 5, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offset2b - 3, rowRulerOutput - offset2s), module, Entropy::PROB_PARAMS + 6, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		addParam(createDynamicParam<GeoKnobTopRight>(Vec(colRulerCenter - offset2s - 7.5f, rowRulerOutput - offset2b + 1.0f), module, Entropy::PROB_PARAMS + 7, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		
		// Blue step lights	
		float radiusBL = 228.5f - 155.5f;// radius blue lights
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerOutput - radiusBL), module, Entropy::STEP_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + radiusBL * 0.707f, rowRulerOutput - radiusBL * 0.707f), module, Entropy::STEP_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + radiusBL, rowRulerOutput), module, Entropy::STEP_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + radiusBL * 0.707f, rowRulerOutput + radiusBL * 0.707f), module, Entropy::STEP_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerOutput + radiusBL), module, Entropy::STEP_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - radiusBL * 0.707f, rowRulerOutput + radiusBL * 0.707f), module, Entropy::STEP_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - radiusBL, rowRulerOutput), module, Entropy::STEP_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - radiusBL * 0.707f, rowRulerOutput - radiusBL * 0.707f), module, Entropy::STEP_LIGHTS + 7));
		radiusBL += 9.0f;
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter, rowRulerOutput - radiusBL), module, Entropy::STEP_LIGHTS + 8 + 0));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + radiusBL * 0.707f, rowRulerOutput - radiusBL * 0.707f), module, Entropy::STEP_LIGHTS + 8 + 1));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + radiusBL, rowRulerOutput), module, Entropy::STEP_LIGHTS + 8 + 2));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + radiusBL * 0.707f, rowRulerOutput + radiusBL * 0.707f), module, Entropy::STEP_LIGHTS + 8 + 3));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter, rowRulerOutput + radiusBL), module, Entropy::STEP_LIGHTS + 8 + 4));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - radiusBL * 0.707f, rowRulerOutput + radiusBL * 0.707f), module, Entropy::STEP_LIGHTS + 8 + 5));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - radiusBL, rowRulerOutput), module, Entropy::STEP_LIGHTS + 8 + 6));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter - radiusBL * 0.707f, rowRulerOutput - radiusBL * 0.707f), module, Entropy::STEP_LIGHTS + 8 + 7));
	
	
		// Length jack, button and lights
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + 116.5f, rowRulerOutput + 70.0f), Port::INPUT, module, Entropy::LENGTH_INPUT, &module->panelTheme));
		static float lenButtonX = colRulerCenter + 130.5f;
		static float lenButtonY = rowRulerOutput + 36.5f;
		addParam(createDynamicParam<GeoPushButton>(Vec(lenButtonX, lenButtonY), module, Entropy::LENGTH_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoRedLight>>(Vec(lenButtonX        , lenButtonY - 14.5f), module, Entropy::LENGTH_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoRedLight>>(Vec(lenButtonX + 10.5f, lenButtonY - 10.5f), module, Entropy::LENGTH_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoRedLight>>(Vec(lenButtonX + 14.5f, lenButtonY        ), module, Entropy::LENGTH_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoRedLight>>(Vec(lenButtonX + 10.5f, lenButtonY + 10.5f), module, Entropy::LENGTH_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<GeoRedLight>>(Vec(lenButtonX        , lenButtonY + 14.5f), module, Entropy::LENGTH_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<GeoRedLight>>(Vec(lenButtonX - 10.5f, lenButtonY + 10.5f), module, Entropy::LENGTH_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<GeoRedLight>>(Vec(lenButtonX - 14.5f, lenButtonY        ), module, Entropy::LENGTH_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<GeoRedLight>>(Vec(lenButtonX - 10.5f, lenButtonY - 10.5f), module, Entropy::LENGTH_LIGHTS + 7));
		
		// Clock inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - 130.5f, rowRulerOutput + 36.5f), Port::INPUT, module, Entropy::CERTAIN_CLK_INPUT, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - 116.5f, rowRulerOutput + 70.0f), Port::INPUT, module, Entropy::UNCERTAIN_CLK_INPUT, &module->panelTheme));
		
		// Switch, add, state (jacks, buttons, ligths)
		// left side
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - 130.5f, rowRulerOutput - 36.0f), Port::INPUT, module, Entropy::STATE_INPUTS + 0, &module->panelTheme));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - 115.5f, rowRulerOutput - 69.0f), module, Entropy::STATE_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - 115.5f - 7.0f, rowRulerOutput - 69.0f + 13.0f), module, Entropy::STATE_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - 115.5f + 3.0f, rowRulerOutput - 69.0f + 14.0f), module, Entropy::ADD_LIGHT + 0));
		// right side
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + 130.5f, rowRulerOutput - 36.0f), Port::INPUT, module, Entropy::STATE_INPUTS + 1, &module->panelTheme));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + 115.5f, rowRulerOutput - 69.0f), module, Entropy::STATE_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + 115.5f + 7.0f, rowRulerOutput - 69.0f + 13.0f), module, Entropy::STATE_LIGHTS + 1));
		
		// Plank constant (jack, light and button)
		// left side (blue)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - 96.0f, rowRulerOutput - 96.0f), Port::INPUT, module, Entropy::QUANTIZE_INPUTS + 0, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - 96.0f - 13.0f, rowRulerOutput - 96.0f - 13.0f), module, Entropy::QUANTIZE_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - 96.0f - 23.0f, rowRulerOutput - 96.0f - 23.0f), module, Entropy::QUANTIZE_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		// right side (yellow)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + 96.0f, rowRulerOutput - 96.0f), Port::INPUT, module, Entropy::QUANTIZE_INPUTS + 1, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + 96.0f + 13.0f, rowRulerOutput - 96.0f - 13.0f), module, Entropy::QUANTIZE_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + 96.0f + 23.0f, rowRulerOutput - 96.0f - 23.0f), module, Entropy::QUANTIZE_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// Energy (button and lights)
		// left side (blue)
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - 69.5f, rowRulerOutput - 116.0f), module, Entropy::OCT_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - 69.5f - 12.0f, rowRulerOutput - 116.0f + 9.0f), module, Entropy::ENERGY_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - 69.5f - 15.0f, rowRulerOutput - 116.0f - 1.0f), module, Entropy::ENERGY_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - 69.5f - 3.0f, rowRulerOutput - 116.0f + 14.0f), module, Entropy::ENERGY_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - 69.5f - 10.0f, rowRulerOutput - 116.0f - 11.0f), module, Entropy::ENERGY_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - 69.5f + 7.0f, rowRulerOutput - 116.0f + 12.0f), module, Entropy::ENERGY_LIGHTS + 2));
		// right side (yellow)
		// left side (blue)
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + 69.5f, rowRulerOutput - 116.0f), module, Entropy::OCT_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + 69.5f + 12.0f, rowRulerOutput - 116.0f + 9.0f), module, Entropy::ENERGY_LIGHTS + 3 + 0));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + 69.5f + 15.0f, rowRulerOutput - 116.0f - 1.0f), module, Entropy::ENERGY_LIGHTS + 3 + 1));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + 69.5f + 3.0f, rowRulerOutput - 116.0f + 14.0f), module, Entropy::ENERGY_LIGHTS + 3 + 1));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + 69.5f + 10.0f, rowRulerOutput - 116.0f - 11.0f), module, Entropy::ENERGY_LIGHTS + 3 + 2));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + 69.5f - 7.0f, rowRulerOutput - 116.0f + 12.0f), module, Entropy::ENERGY_LIGHTS + 3 + 2));
		
		
		// Top portion
		static constexpr float rowRulerTop = rowRulerOutput - 149.5f;
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerTop - 30.5f), Port::INPUT, module, Entropy::GPROB_INPUT, &module->panelTheme));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerTop), module, Entropy::GPROB_PARAM, 0.0f, 1.0f, 0.5f, &module->panelTheme));
		
		// Left side top
		// ext
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - 77.5f, rowRulerTop), Port::INPUT, module, Entropy::EXTSIG_INPUTS + 0, &module->panelTheme));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - 41.5f, rowRulerTop), module, Entropy::EXTSIG_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - 26.5f, rowRulerTop), module, Entropy::EXTSIG_LIGHTS + 0));
		// random
		static constexpr float buttonOffsetX = 35.5f;// button
		static constexpr float buttonOffsetY = 20.5f;// button
		static constexpr float lightOffsetX = 22.5f;// light
		static constexpr float lightOffsetY = 12.5f;// light
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - buttonOffsetX, rowRulerTop - buttonOffsetY), module, Entropy::RANDOM_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - lightOffsetX, rowRulerTop - lightOffsetY), module, Entropy::RANDOM_LIGHTS + 0));
		// fixed cv
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - buttonOffsetX, rowRulerTop + buttonOffsetY), module, Entropy::FIXEDCV_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - lightOffsetX, rowRulerTop + lightOffsetY), module, Entropy::FIXEDCV_LIGHTS + 0));
		
		// Right side top
		// ext
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + 77.5f, rowRulerTop), Port::INPUT, module, Entropy::EXTSIG_INPUTS + 1, &module->panelTheme));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + 41.5f, rowRulerTop), module, Entropy::EXTSIG_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + 26.5f, rowRulerTop), module, Entropy::EXTSIG_LIGHTS + 1));
		// random
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + buttonOffsetX, rowRulerTop - buttonOffsetY), module, Entropy::RANDOM_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + lightOffsetX, rowRulerTop - lightOffsetY), module, Entropy::RANDOM_LIGHTS + 1));
		// fixed cv
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + buttonOffsetX, rowRulerTop + buttonOffsetY), module, Entropy::FIXEDCV_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoYellowLight>>(Vec(colRulerCenter + lightOffsetX, rowRulerTop + lightOffsetY), module, Entropy::FIXEDCV_LIGHTS + 1));
		
		
		
		// Bottom row
		
		// Run jack, light and button
		static constexpr float rowRulerRunJack = 380.0f - 32.5f;
		static constexpr float offsetRunJackX = 119.5f;
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetRunJackX, rowRulerRunJack), Port::INPUT, module, Entropy::RUN_INPUT, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetRunJackX + 18.0f, rowRulerRunJack), module, Entropy::RUN_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetRunJackX + 33.0f, rowRulerRunJack), module, Entropy::RUN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		
		// Reset jack, light and button
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetRunJackX, rowRulerRunJack), Port::INPUT, module, Entropy::RESET_INPUT, &module->panelTheme));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetRunJackX - 18.0f, rowRulerRunJack), module, Entropy::RESET_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetRunJackX - 33.0f, rowRulerRunJack), module, Entropy::RESET_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
	
		static constexpr float offsetMagneticButton = 42.5f;
		// Magnetic clock (step clocks)
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetMagneticButton - 15.0f, rowRulerRunJack), module, Entropy::STEPCLOCK_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetMagneticButton, rowRulerRunJack), module, Entropy::STEPCLOCK_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));			
		// Reset on Run light and button
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetMagneticButton + 15.0f, rowRulerRunJack), module, Entropy::RESETONRUN_LIGHT));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetMagneticButton, rowRulerRunJack), module, Entropy::RESETONRUN_PARAM, 0.0f, 1.0f, 0.0f, &module->panelTheme));	
		
		
	}
};

Model *modelEntropy = Model::create<Entropy, EntropyWidget>("Geodesics", "Entropy", "Entropy", SEQUENCER_TAG);

/*CHANGE LOG

0.6.5:
created

*/