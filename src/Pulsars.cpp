//***********************************************************************************************
//Neutron-Powered Crossfader module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"


struct Pulsars : Module {
	enum ParamIds {
		ENUMS(VOID_PARAMS, 2),// push-button
		ENUMS(REV_PARAMS, 2),// push-button
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(INA_INPUTS, 8),
		INB_INPUT,
		ENUMS(VOID_INPUTS, 2),
		ENUMS(REV_INPUTS, 2),
		ENUMS(LFO_INPUTS, 2),
		NUM_INPUTS
	};
	enum OutputIds {
		OUTA_OUTPUT,
		ENUMS(OUTB_OUTPUTS, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(MIXA_LIGHTS, 8),
		ENUMS(MIXB_LIGHTS, 8),
		ENUMS(VOID_LIGHTS, 2),
		ENUMS(REV_LIGHTS, 2),
		NUM_LIGHTS
	};
	
	
	// Constants
	// none

	// Need to save, with reset
	bool isVoid[2];
	bool isReverse[2];
	
	// Need to save, no reset
	int panelTheme;
	
	// No need to save, with reset
	int posA;// always between 0 and 7
	
	// No need to save, no reset
	SchmittTrigger voidTriggers[2];
	SchmittTrigger revTriggers[2];

	
	Pulsars() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		// No need to save, no reset		
		for (int i = 0; i < 2; i++) {
			voidTriggers[i].reset();
			revTriggers[i].reset();
		}
		
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
			isVoid[i] = false;
			isReverse[i] = false;
		}
		// No need to save, with reset
		posA = 0;
	}

	
	// widgets randomized before onRandomize() is called
	// called by engine thread if right-click randomize
	void onRandomize() override {
		// Need to save, with reset
		for (int i = 0; i < 2; i++) {
			isVoid[i] = (randomu32() % 2) > 0;
			isReverse[i] = (randomu32() % 2) > 0;
		}
		// No need to save, with reset
		posA = randomu32() % 8;
	}

	
	// called by main thread
	json_t *toJson() override {
		json_t *rootJ = json_object();
		// Need to save (reset or not)

		// isVoid
		json_object_set_new(rootJ, "isVoid0", json_real(isVoid[0]));
		json_object_set_new(rootJ, "isVoid1", json_real(isVoid[1]));
		
		// isReverse
		json_object_set_new(rootJ, "isReverse0", json_real(isReverse[0]));
		json_object_set_new(rootJ, "isReverse1", json_real(isReverse[1]));
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}

	
	// widgets have their fromJson() called before this fromJson() is called
	// called by main thread
	void fromJson(json_t *rootJ) override {
		// Need to save (reset or not)

		// isVoid
		json_t *isVoid0J = json_object_get(rootJ, "isVoid0");
		if (isVoid0J)
			isVoid[0] = json_real_value(isVoid0J);
		json_t *isVoid1J = json_object_get(rootJ, "isVoid1");
		if (isVoid1J)
			isVoid[1] = json_real_value(isVoid1J);

		// isReverse
		json_t *isReverse0J = json_object_get(rootJ, "isReverse0");
		if (isReverse0J)
			isReverse[0] = json_real_value(isReverse0J);
		json_t *isReverse1J = json_object_get(rootJ, "isReverse1");
		if (isReverse1J)
			isReverse[1] = json_real_value(isReverse1J);

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// No need to save, with reset
		// none
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void step() override {		
		// Void and Reverse buttons
		for (int i = 0; i < 2; i++) {
			if (voidTriggers[i].process(params[VOID_PARAMS + i].value + inputs[VOID_INPUTS + i].value)) {
				isVoid[i] = !isVoid[i];
			}
			if (revTriggers[i].process(params[REV_PARAMS + i].value + inputs[REV_INPUTS + i].value)) {
				isReverse[i] = !isReverse[i];
			}
		}
		
		// Pulsar A
		bool active8[8];
		float values8[8];
		for (int i = 0; i < 8; i++) {
			active8[i] = inputs[INA_INPUTS + i].active;
			values8[i] = inputs[INA_INPUTS + i].value;
		}
		int newPosA = getClosestActive(posA, active8, false);
		if (newPosA != -1) {// if at least one active input
			posA = newPosA;
			int posAnext = (posA + 1) % 8;
			if (!isVoid[0])
				posAnext = getClosestActive(posAnext, active8, isReverse[0]);
			
			float lfo01 = (inputs[LFO_INPUTS + 0].value + 5.0f) / 10.0f;
			outputs[OUTA_OUTPUT].value = lfo01 * values8[posA] + (1.0f - lfo01) * values8[posAnext];
			for (int i = 0; i < 8; i++)
				lights[MIXA_LIGHTS + i].value = (i == posA) ? lfo01 : (i == posAnext ? (1.0f - lfo01) : 0.0f);
		}
		else {
			outputs[OUTA_OUTPUT].value = 0.0f;
			for (int i = 0; i < 8; i++)
				lights[MIXA_LIGHTS + i].value = 0.0f;
		}
		


				
		// Void and Reverse lights
		for (int i = 0; i < 2; i++) {
			lights[VOID_LIGHTS + i].value = isVoid[i] ? 1.0f : 0.0f;
			lights[REV_LIGHTS + i].value = isReverse[i] ? 1.0f : 0.0f;
		}
	}// step()
	
	int getClosestActive(int pos, bool* active8, bool reverse) {
		// finds the closest active position (including current if active), returns -1 if none are active
		// starts in current pos and looks at 8 positions starting here (loops over if given pos > 1)
		int ret = -1;
		if (!reverse) {
			for (int i = pos; i < pos + 8; i++) {
				if (active8[i % 8]) {
					ret = i % 8;
					break;
				}
			}
		}
		else {
			for (int i = pos + 8; i > pos; i--) {
				if (active8[i % 8]) {
					ret = i % 8;
					break;
				}
			}
		}
		return ret;
	}

};


struct PulsarsWidget : ModuleWidget {

	struct PanelThemeItem : MenuItem {
		Pulsars *module;
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

		Pulsars *module = dynamic_cast<Pulsars*>(this->module);
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
	
	PulsarsWidget(Pulsars *module) : ModuleWidget(module) {
		// Main panel from Inkscape
        DynamicSVGPanel *panel = new DynamicSVGPanel();
        panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/Pulsars.svg")));
        //panel->addPanel(SVG::load(assetPlugin(plugin, "res/light/Pulsars_dark.svg")));// no dark pannel for now
        box.size = panel->box.size;
        panel->mode = &module->panelTheme;
        addChild(panel);

		// Screws 
		// part of svg panel, no code required
		
		float colRulerCenter = box.size.x / 2.0f;
		float rowRulerPulsarA = 108.5f;
		float rowRulerPulsarB = 272.5f;
		float radiusLeds = 25.0f;
		float radiusJacks = 50.0f;
		float offsetLeds = radiusLeds / 1.4142135f;
		float offsetJacks = radiusJacks / 1.4142135f;
		float offsetLFO = 25.0f;
		

		// PulsarA center output
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA), Port::OUTPUT, module, Pulsars::OUTA_OUTPUT, &module->panelTheme));
		
		// PulsarA inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA - radiusJacks), Port::INPUT, module, Pulsars::INA_INPUTS + 0, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarA - offsetJacks), Port::INPUT, module, Pulsars::INA_INPUTS + 1, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusJacks, rowRulerPulsarA), Port::INPUT, module, Pulsars::INA_INPUTS + 2, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarA + offsetJacks), Port::INPUT, module, Pulsars::INA_INPUTS + 3, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA + radiusJacks), Port::INPUT, module, Pulsars::INA_INPUTS + 4, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarA + offsetJacks), Port::INPUT, module, Pulsars::INA_INPUTS + 5, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusJacks, rowRulerPulsarA), Port::INPUT, module, Pulsars::INA_INPUTS + 6, &module->panelTheme));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarA - offsetJacks), Port::INPUT, module, Pulsars::INA_INPUTS + 7, &module->panelTheme));

		// PulsarA lights
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter, rowRulerPulsarA - radiusLeds), module, Pulsars::MIXA_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarA - offsetLeds), module, Pulsars::MIXA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + radiusLeds, rowRulerPulsarA), module, Pulsars::MIXA_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarA + offsetLeds), module, Pulsars::MIXA_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter, rowRulerPulsarA + radiusLeds), module, Pulsars::MIXA_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarA + offsetLeds), module, Pulsars::MIXA_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - radiusLeds, rowRulerPulsarA), module, Pulsars::MIXA_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarA - offsetLeds), module, Pulsars::MIXA_LIGHTS + 7));
		
		// PulsarA LFO input
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarA + offsetJacks + offsetLFO), Port::INPUT, module, Pulsars::LFO_INPUTS + 0, &module->panelTheme));
		
		
		
		// PulsarB center input
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB), Port::INPUT, module, Pulsars::INB_INPUT, &module->panelTheme));
		
		// PulsarB outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB - radiusJacks), Port::OUTPUT, module, Pulsars::OUTB_OUTPUTS + 0, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarB - offsetJacks), Port::OUTPUT, module, Pulsars::OUTB_OUTPUTS + 1, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusJacks, rowRulerPulsarB), Port::OUTPUT, module, Pulsars::OUTB_OUTPUTS + 2, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarB + offsetJacks), Port::OUTPUT, module, Pulsars::OUTB_OUTPUTS + 3, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB + radiusJacks), Port::OUTPUT, module, Pulsars::OUTB_OUTPUTS + 4, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarB + offsetJacks), Port::OUTPUT, module, Pulsars::OUTB_OUTPUTS + 5, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusJacks, rowRulerPulsarB), Port::OUTPUT, module, Pulsars::OUTB_OUTPUTS + 6, &module->panelTheme));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarB - offsetJacks), Port::OUTPUT, module, Pulsars::OUTB_OUTPUTS + 7, &module->panelTheme));

		// PulsarB lights
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter, rowRulerPulsarB - radiusLeds), module, Pulsars::MIXA_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarB - offsetLeds), module, Pulsars::MIXA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + radiusLeds, rowRulerPulsarB), module, Pulsars::MIXA_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarB + offsetLeds), module, Pulsars::MIXA_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter, rowRulerPulsarB + radiusLeds), module, Pulsars::MIXA_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarB + offsetLeds), module, Pulsars::MIXA_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - radiusLeds, rowRulerPulsarB), module, Pulsars::MIXA_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarB - offsetLeds), module, Pulsars::MIXA_LIGHTS + 7));
		
		// PulsarA LFO input
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarB - offsetJacks - offsetLFO), Port::INPUT, module, Pulsars::LFO_INPUTS + 1, &module->panelTheme));		
	
	}
};

Model *modelPulsars = Model::create<Pulsars, PulsarsWidget>("Geodesics", "Pulsars", "Pulsars", MIXER_TAG);

/*CHANGE LOG

0.6.0:
created

*/