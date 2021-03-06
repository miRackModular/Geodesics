//***********************************************************************************************
//Gravitational Voltage Controled Amplifiers module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"


struct BlackHoles : Module {
	enum ParamIds {
		ENUMS(LEVEL_PARAMS, 8),// -1.0f to 1.0f knob, set to default (0.0f) when using CV input
		ENUMS(EXP_PARAMS, 2),// push-button
		WORMHOLE_PARAM,
		ENUMS(CVLEVEL_PARAMS, 2),// push-button
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(IN_INPUTS, 8),// -10 to 10 V 
		ENUMS(LEVELCV_INPUTS, 8),// 0 to 10V CV or -5 to 5V depeding on cvMode
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, 8),// input * [-1;1] when input connected, else [-10;10] CV when input unconnected
		ENUMS(BLACKHOLE_OUTPUTS, 2),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(EXP_LIGHTS, 2),
		WORMHOLE_LIGHT,
		ENUMS(CVALEVEL_LIGHTS, 2),// White, but two lights (light 0 is cvMode bit = 0, light 1 is cvMode bit = 1)
		ENUMS(CVBLEVEL_LIGHTS, 2),// White, but two lights
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float expBase = 50.0f;

	
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	bool isExponential[2];
	bool wormhole;
	int cvMode;// 0 is -5v to 5v, 1 is -10v to 10v; bit 0 is upper BH, bit 1 is lower BH
	
	// No need to save, with reset
	// none
	
	// No need to save, no reset
	Trigger expTriggers[2];
	Trigger cvLevelTriggers[2];
	Trigger wormholeTrigger;
	RefreshCounter refresh;

	
	BlackHoles() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(LEVEL_PARAMS + 0, -1.0f, 1.0f, 0.0f, "Top BH level 1");
		configParam(LEVEL_PARAMS + 1, -1.0f, 1.0f, 0.0f, "Top BH level 2");
		configParam(LEVEL_PARAMS + 2, -1.0f, 1.0f, 0.0f, "Top BH level 3");
		configParam(LEVEL_PARAMS + 3, -1.0f, 1.0f, 0.0f, "Top BH level 4");
		configParam(LEVEL_PARAMS + 4, -1.0f, 1.0f, 0.0f, "Button BH level 1");
		configParam(LEVEL_PARAMS + 5, -1.0f, 1.0f, 0.0f, "Button BH level 2");
		configParam(LEVEL_PARAMS + 6, -1.0f, 1.0f, 0.0f, "Button BH level 3");
		configParam(LEVEL_PARAMS + 7, -1.0f, 1.0f, 0.0f, "Button BH level 4");
		configParam(EXP_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top BH exponential");
		configParam(EXP_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom BH exponential");
		configParam(WORMHOLE_PARAM, 0.0f, 1.0f, 0.0f, "Wormhole");
		configParam(CVLEVEL_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top BH gravity");
		configParam(CVLEVEL_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom BH gravity");		
		
		onReset();		
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		isExponential[0] = false;
		isExponential[1] = false;
		wormhole = true;
		cvMode = 0x3;
		// resetNonJson
	}
	// void resetNonJson() {
		// none
	// }

	
	void onRandomize() override {
		for (int i = 0; i < 2; i++) {
			isExponential[i] = (random::u32() % 2) > 0;
		}
		wormhole = (random::u32() % 2) > 0;
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// isExponential
		json_object_set_new(rootJ, "isExponential0", json_real(isExponential[0]));
		json_object_set_new(rootJ, "isExponential1", json_real(isExponential[1]));
		
		// wormhole
		json_object_set_new(rootJ, "wormhole", json_boolean(wormhole));

		// cvMode
		json_object_set_new(rootJ, "cvMode", json_integer(cvMode));

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// isExponential
		json_t *isExponential0J = json_object_get(rootJ, "isExponential0");
		if (isExponential0J)
			isExponential[0] = json_number_value(isExponential0J);
		json_t *isExponential1J = json_object_get(rootJ, "isExponential1");
		if (isExponential1J)
			isExponential[1] = json_number_value(isExponential1J);

		// wormhole
		json_t *wormholeJ = json_object_get(rootJ, "wormhole");
		if (wormholeJ)
			wormhole = json_is_true(wormholeJ);

		// cvMode
		json_t *cvModeJ = json_object_get(rootJ, "cvMode");
		if (cvModeJ)
			cvMode = json_integer_value(cvModeJ);
		
		// resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {
		if (refresh.processInputs()) {
			// Exponential buttons
			for (int i = 0; i < 2; i++)
				if (expTriggers[i].process(params[EXP_PARAMS + i].getValue())) {
					isExponential[i] = !isExponential[i];
			}
			
			// Wormhole buttons
			if (wormholeTrigger.process(params[WORMHOLE_PARAM].getValue())) {
				wormhole = ! wormhole;
			}

			// CV Level buttons
			for (int i = 0; i < 2; i++) {
				if (cvLevelTriggers[i].process(params[CVLEVEL_PARAMS + i].getValue()))
					cvMode ^= (0x1 << i);
			}
		}// userInputs refresh
		
		// BlackHole 0 all outputs
		float blackHole0 = 0.0f;
		float inputs0[4] = {10.0f, 10.0f, 10.0f, 10.0f};// default to generate CV when no input connected
		for (int i = 0; i < 4; i++) 
			if (inputs[IN_INPUTS + i].isConnected())
				inputs0[i] = inputs[IN_INPUTS + i].getVoltage();
		for (int i = 0; i < 4; i++) {
			float chanVal = calcChannel(inputs0[i], params[LEVEL_PARAMS + i], inputs[LEVELCV_INPUTS + i], isExponential[0], cvMode & 0x1);
			outputs[OUT_OUTPUTS + i].setVoltage(chanVal);
			blackHole0 += chanVal;
		}
		outputs[BLACKHOLE_OUTPUTS + 0].setVoltage(clamp(blackHole0, -10.0f, 10.0f));
			
			
		// BlackHole 1 all outputs
		float blackHole1 = 0.0f;
		float inputs1[4] = {10.0f, 10.0f, 10.0f, 10.0f};// default to generate CV when no input connected
		for (int i = 0; i < 4; i++) {
			if (inputs[IN_INPUTS + i + 4].isConnected())
				inputs1[i] = inputs[IN_INPUTS + i + 4].getVoltage();
			else if (wormhole)
				inputs1[i] = blackHole0;
		}
		for (int i = 0; i < 4; i++) {
			float chanVal = calcChannel(inputs1[i], params[LEVEL_PARAMS + i + 4], inputs[LEVELCV_INPUTS + i + 4], isExponential[1], cvMode >> 1);
			outputs[OUT_OUTPUTS + i + 4].setVoltage(chanVal);
			blackHole1 += chanVal;
		}
		outputs[BLACKHOLE_OUTPUTS + 1].setVoltage(clamp(blackHole1, -10.0f, 10.0f));

		// lights
		if (refresh.processLights()) {
			// Wormhole light
			lights[WORMHOLE_LIGHT].setBrightness(wormhole ? 1.0f : 0.0f);
					
			// isExponential lights
			for (int i = 0; i < 2; i++)
				lights[EXP_LIGHTS + i].setBrightness(isExponential[i] ? 1.0f : 0.0f);
			
			// CV Level lights
			bool is5V = (cvMode & 0x1) == 0;
			lights[CVALEVEL_LIGHTS + 0].setBrightness(is5V ? 1.0f : 0.0f);
			lights[CVALEVEL_LIGHTS + 1].setBrightness(is5V ? 0.0f : 1.0f);
			is5V = (cvMode & 0x2) == 0;
			lights[CVBLEVEL_LIGHTS + 0].setBrightness(is5V ? 1.0f : 0.0f);
			lights[CVBLEVEL_LIGHTS + 1].setBrightness(is5V ? 0.0f : 1.0f);

		}// lightRefreshCounter
		
	}// step()
	
	inline float calcChannel(float in, Param &level, Input &levelCV, bool isExp, int cvMode) {
		float levCv = levelCV.isConnected() ? (levelCV.getVoltage() * (cvMode != 0 ? 0.1f : 0.2f)) : 0.0f;
		float lev = clamp(level.getValue() + levCv, -1.0f, 1.0f);
		if (isExp) {
			float newlev = rescale(std::pow(expBase, std::fabs(lev)), 1.0f, expBase, 0.0f, 1.0f);
			if (lev < 0.0f)
				newlev *= -1.0f;
			lev = newlev;
		}
		float ret = lev * in;
		return ret;
	}	
};


struct BlackHolesWidget : ModuleWidget {
	BlackHolesWidget(BlackHoles *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/BlackHoles-DM.svg")));
		
		// Screws 
		// part of svg panel, no code required
		
		float colRulerCenter = box.size.x / 2.0f;
		static constexpr float rowRulerBlack0 = 108.5f;
		static constexpr float rowRulerBlack1 = 272.5f;
		static constexpr float radiusIn = 30.0f;
		static constexpr float radiusOut = 61.0f;
		static constexpr float offsetL = 53.0f;
		static constexpr float offsetS = 30.0f;
		
		
		// BlackHole0 knobs
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerBlack0 - radiusOut), module, BlackHoles::LEVEL_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoKnobRight>(Vec(colRulerCenter + radiusOut, rowRulerBlack0), module, BlackHoles::LEVEL_PARAMS + 1, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoKnobBottom>(Vec(colRulerCenter, rowRulerBlack0 + radiusOut), module, BlackHoles::LEVEL_PARAMS + 2, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoKnobLeft>(Vec(colRulerCenter - radiusOut, rowRulerBlack0), module, BlackHoles::LEVEL_PARAMS + 3, module ? &module->panelTheme : NULL));

		// BlackHole0 level CV inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerBlack0 - radiusIn), true, module, BlackHoles::LEVELCV_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusIn, rowRulerBlack0), true, module, BlackHoles::LEVELCV_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerBlack0 + radiusIn), true, module, BlackHoles::LEVELCV_INPUTS + 2, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusIn, rowRulerBlack0), true, module, BlackHoles::LEVELCV_INPUTS + 3, module ? &module->panelTheme : NULL));

		// BlackHole0 inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetS, rowRulerBlack0 - offsetL), true, module, BlackHoles::IN_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetL, rowRulerBlack0 - offsetS), true, module, BlackHoles::IN_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetS, rowRulerBlack0 + offsetL), true, module, BlackHoles::IN_INPUTS + 2, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetL, rowRulerBlack0 + offsetS), true, module, BlackHoles::IN_INPUTS + 3, module ? &module->panelTheme : NULL));
		
		// BlackHole0 outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetS, rowRulerBlack0 - offsetL), false, module, BlackHoles::OUT_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetL, rowRulerBlack0 + offsetS), false, module, BlackHoles::OUT_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetS, rowRulerBlack0 + offsetL), false, module, BlackHoles::OUT_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetL, rowRulerBlack0 - offsetS), false, module, BlackHoles::OUT_OUTPUTS + 3, module ? &module->panelTheme : NULL));
		// BlackHole0 center output
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerBlack0), false, module, BlackHoles::BLACKHOLE_OUTPUTS + 0, module ? &module->panelTheme : NULL));

		// BlackHole1 knobs
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter, rowRulerBlack1 - radiusOut), module, BlackHoles::LEVEL_PARAMS + 4, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoKnobRight>(Vec(colRulerCenter + radiusOut, rowRulerBlack1), module, BlackHoles::LEVEL_PARAMS + 5, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoKnobBottom>(Vec(colRulerCenter, rowRulerBlack1 + radiusOut), module, BlackHoles::LEVEL_PARAMS + 6, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoKnobLeft>(Vec(colRulerCenter - radiusOut, rowRulerBlack1), module, BlackHoles::LEVEL_PARAMS + 7, module ? &module->panelTheme : NULL));
		
		// BlackHole1 level CV inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerBlack1 - radiusIn), true, module, BlackHoles::LEVELCV_INPUTS + 4, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusIn, rowRulerBlack1), true, module, BlackHoles::LEVELCV_INPUTS + 5, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerBlack1 + radiusIn), true, module, BlackHoles::LEVELCV_INPUTS + 6, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusIn, rowRulerBlack1), true, module, BlackHoles::LEVELCV_INPUTS + 7, module ? &module->panelTheme : NULL));

		// BlackHole1 inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetS, rowRulerBlack1 - offsetL), true, module, BlackHoles::IN_INPUTS + 4, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetL, rowRulerBlack1 - offsetS), true, module, BlackHoles::IN_INPUTS + 5, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetS, rowRulerBlack1 + offsetL), true, module, BlackHoles::IN_INPUTS + 6, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetL, rowRulerBlack1 + offsetS), true, module, BlackHoles::IN_INPUTS + 7, module ? &module->panelTheme : NULL));
		
		// BlackHole1 outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetS, rowRulerBlack1 - offsetL), false, module, BlackHoles::OUT_OUTPUTS + 4, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetL, rowRulerBlack1 + offsetS), false, module, BlackHoles::OUT_OUTPUTS + 5, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetS, rowRulerBlack1 + offsetL), false, module, BlackHoles::OUT_OUTPUTS + 6, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetL, rowRulerBlack1 - offsetS), false, module, BlackHoles::OUT_OUTPUTS + 7, module ? &module->panelTheme : NULL));
		// BlackHole1 center output
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerBlack1), false, module, BlackHoles::BLACKHOLE_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		
		
		static constexpr float offsetButtonsX = 62.0f;
		static constexpr float offsetButtonsY = 64.0f;
		static constexpr float offsetLedVsBut = 9.0f;
		static constexpr float offsetLedVsButS = 5.0f;// small
		static constexpr float offsetLedVsButL = 12.0f;// large
		
		
		// BlackHole0 Exp button and light
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetButtonsX, rowRulerBlack0 + offsetButtonsY), module, BlackHoles::EXP_PARAMS + 0, module ? &module->panelTheme : NULL));	
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetButtonsX + offsetLedVsBut, rowRulerBlack0 + offsetButtonsY - offsetLedVsBut - 1.0f), module, BlackHoles::EXP_LIGHTS + 0));
		
		// BlackHole1 Exp button and light
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetButtonsX, rowRulerBlack1 + offsetButtonsY), module, BlackHoles::EXP_PARAMS + 1, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetButtonsX + offsetLedVsBut, rowRulerBlack1 + offsetButtonsY - offsetLedVsBut -1.0f), module, BlackHoles::EXP_LIGHTS + 1));
		
		// Wormhole button and light
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetButtonsX, rowRulerBlack1 - offsetButtonsY), module, BlackHoles::WORMHOLE_PARAM, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetButtonsX + offsetLedVsBut, rowRulerBlack1 - offsetButtonsY + offsetLedVsBut), module, BlackHoles::WORMHOLE_LIGHT));
		
		
		// CV Level A button and light
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetButtonsX, rowRulerBlack0 + offsetButtonsY), module, BlackHoles::CVLEVEL_PARAMS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetButtonsX + offsetLedVsButL, rowRulerBlack0 + offsetButtonsY + offsetLedVsButS), module, BlackHoles::CVALEVEL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetButtonsX + offsetLedVsButS, rowRulerBlack0 + offsetButtonsY + offsetLedVsButL), module, BlackHoles::CVALEVEL_LIGHTS + 1));
		
		// CV Level B button and light
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetButtonsX, rowRulerBlack1 + offsetButtonsY), module, BlackHoles::CVLEVEL_PARAMS + 1, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetButtonsX + offsetLedVsButL, rowRulerBlack1 + offsetButtonsY + offsetLedVsButS), module, BlackHoles::CVBLEVEL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetButtonsX + offsetLedVsButS, rowRulerBlack1 + offsetButtonsY + offsetLedVsButL), module, BlackHoles::CVBLEVEL_LIGHTS + 1));
	}
};

Model *modelBlackHoles = createModel<BlackHoles, BlackHolesWidget>("BlackHoles");

/*CHANGE LOG


*/