//***********************************************************************************************
//Gravitational VCAs module for VCV Rack by Pierre Collard and Marc Boulé
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
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(IN_INPUTS, 8),// -10 to 10 V 
		ENUMS(LEVELCV_INPUTS, 8),// 0 to 10V CV
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, 8),// input * [-1;1] when input connected, else [-10;10] CV when input unconnected
		ENUMS(BLACKHOLE_OUTPUTS, 2),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(EXP_LIGHTS, 2),
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float expBase = 50.0f;

	// Need to save, with reset
	bool isExponential[2];
	
	// Need to save, no reset
	int panelTheme;
	
	// No need to save, with reset
	// none
	
	// No need to save, no reset
	SchmittTrigger expTriggers[2];

	
	BlackHoles() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		// No need to save, no reset		
		expTriggers[0].reset();
		
		onReset();
	}

	
	// widgets are not yet created when module is created 
	// even if widgets not created yet, can use params[] and should handle 0.0f value since step may call 
	//   this before widget creation anyways
	// called from the main thread if by constructor, called by engine thread if right-click initialization
	//   when called by constructor, module is created before the first step() is called
	void onReset() override {
		// Need to save, with reset
		for (int i = 0; i < 2; i++) {
			isExponential[i] = false;
		}
		// No need to save, with reset
		// none
	}

	
	// widgets randomized before onRandomize() is called
	// called by engine thread if right-click randomize
	void onRandomize() override {
		// Need to save, with reset
		for (int i = 0; i < 2; i++) {
			isExponential[i] = (randomu32() % 2) > 0;
		}
		// No need to save, with reset
		// none
	}

	
	// called by main thread
	json_t *toJson() override {
		json_t *rootJ = json_object();
		// Need to save (reset or not)

		// isExponential
		json_object_set_new(rootJ, "isExponential0", json_real(isExponential[0]));
		json_object_set_new(rootJ, "isExponential1", json_real(isExponential[1]));
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}

	
	// widgets have their fromJson() called before this fromJson() is called
	// called by main thread
	void fromJson(json_t *rootJ) override {
		// Need to save (reset or not)

		// isExponential
		json_t *isExponential0J = json_object_get(rootJ, "isExponential0");
		if (isExponential0J)
			isExponential[0] = json_real_value(isExponential0J);
		json_t *isExponential1J = json_object_get(rootJ, "isExponential1");
		if (isExponential1J)
			isExponential[1] = json_real_value(isExponential1J);

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// No need to save, with reset
		// none
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		// Exponential buttons
		for (int i = 0; i < 2; i++) {
			if (expTriggers[i].process(params[EXP_PARAMS + i].value)) {
				isExponential[i] = !isExponential[i];
			}
		}
		
		
		// BlackHole 0 all outputs
		float blackHole0 = 0.0f;
		float inputs0[4] = {10.0f, 10.0f, 10.0f, 10.0f};// default to generate CV when no input connected
		for (int i = 0; i < 4; i++) 
			if (inputs[IN_INPUTS + i].active)
				inputs0[i] = inputs[IN_INPUTS + i].value;
		for (int i = 0; i < 4; i++) {
			float chanVal = calcChannel(inputs0[i], params[LEVEL_PARAMS + i], inputs[LEVELCV_INPUTS + i], isExponential[0]);
			outputs[OUT_OUTPUTS + i].value = chanVal;
			blackHole0 += chanVal;
		}
		outputs[BLACKHOLE_OUTPUTS + 0].value = blackHole0;
			
			
		// BlackHole 1 all outputs
		float blackHole1 = 0.0f;
		float inputs1[4] = {10.0f, 10.0f, 10.0f, 10.0f};// default to generate CV when no input connected
		bool allUnconnected = true;
		for (int i = 0; i < 4; i++) 
			if (inputs[IN_INPUTS + i + 4].active) {
				inputs1[i] = inputs[IN_INPUTS + i + 4].value;
				allUnconnected = false;
			}
		if (allUnconnected)
			for (int i = 0; i < 4; i++)
				inputs1[i] = blackHole0;
		for (int i = 0; i < 4; i++) {
			float chanVal = calcChannel(inputs1[i], params[LEVEL_PARAMS + i + 4], inputs[LEVELCV_INPUTS + i + 4], isExponential[1]);
			outputs[OUT_OUTPUTS + i + 4].value = chanVal;
			blackHole1 += chanVal;
		}
		outputs[BLACKHOLE_OUTPUTS + 1].value = blackHole1;


				
		// isExponential lights
		for (int i = 0; i < 2; i++)
			lights[EXP_LIGHTS + i].value = isExponential[i] ? 1.0f : 0.0f;
	}
	
	static float calcChannel(float in, Param &level, Input &levelCV, bool isExp) {
		float levCv = levelCV.active ? (levelCV.value / 5.0f - 1.0f) : 0.0f;
		float lev = clamp(level.value + levCv, -1.0f, 1.0f);
		if (isExp) {
			float newlev = rescale(powf(expBase, fabs(lev)), 1.0f, expBase, 0.0f, 1.0f);
			if (lev < 0.0f)
				newlev *= -1.0f;
			lev = newlev;
		}
		float ret = lev * in;
		return ret;
	}	
};


struct BlackHolesWidget : ModuleWidget {

	struct PanelThemeItem : MenuItem {
		BlackHoles *module;
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

		BlackHoles *module = dynamic_cast<BlackHoles*>(this->module);
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
	
	BlackHolesWidget(BlackHoles *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/BlackHoles.svg")));
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/BlackHoles.svg")));
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screws
		addChild(createDynamicScrew<GeoScrew>(Vec(15, 0), &module->panelTheme));
		addChild(createDynamicScrew<GeoScrew>(Vec(box.size.x-30, 0), &module->panelTheme));
		addChild(createDynamicScrew<GeoScrew>(Vec(15, 365), &module->panelTheme));
		addChild(createDynamicScrew<GeoScrew>(Vec(box.size.x-30, 365), &module->panelTheme));
		
		
		float colRulerCenter = box.size.x / 2.0f;
		float rowRulerBlack0 = 100.0f;
		float rowRulerBlack1 = 280.0f;
		float radiusIn = 40.0f;
		float radiusOut = 80.0f;
		
		// BlackHole0 output
		//addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerBlack0), Port::OUTPUT, module, BlackHoles::BLACKHOLE_OUTPUTS + 0, &module->panelTheme));
		
		// BlackHole0 knobs
		//addParam(createParamCentered<GeoKnob>(Vec(colRulerCenter, rowRulerBlack0 - radiusOut), module, BlackHoles::LEVEL_PARAMS + 0, -1.0f, 1.0f, 0.0f));
		//addParam(createParamCentered<GeoKnobRight>(Vec(colRulerCenter + radiusOut, rowRulerBlack0), module, BlackHoles::LEVEL_PARAMS + 1, -1.0f, 1.0f, 0.0f));
		//addParam(createParamCentered<GeoKnobBottom>(Vec(colRulerCenter, rowRulerBlack0 + radiusOut), module, BlackHoles::LEVEL_PARAMS + 2, -1.0f, 1.0f, 0.0f));
		//addParam(createParamCentered<GeoKnobLeft>(Vec(colRulerCenter - radiusOut, rowRulerBlack0), module, BlackHoles::LEVEL_PARAMS + 3, -1.0f, 1.0f, 0.0f));

		//addChild(ModuleLightWidget::create<MediumLight<GreenRedLight>>(Vec(colRulerLedL, rowRuler0 + lightsOffsetY + i * lightsSpacingY), module, Tact::TACT_LIGHTS + i * 2));
	}
};

Model *modelBlackHoles = Model::create<BlackHoles, BlackHolesWidget>("Geodesics", "BlackHoles", "BlackHoles", AMPLIFIER_TAG);

/*CHANGE LOG

0.6.0:
created

*/