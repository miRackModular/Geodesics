//***********************************************************************************************
//Neutron-Powered Crossfader module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************

//TODO:
//1) right click option for 0 to 10 LFO CV vs -5 to 5
//2) extra light to indicate that a crossover occurred
//3) if LFOB is not connected, LFOA is used instead
//4) if pulsarB has no input nor LFOB, then each output is the corresponding input in PulsarA
//   but attenuated according to LFOB

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
		ENUMS(LFO_LIGHTS, 2),
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float epsilon = 0.001f;// pulsar crossovers at 5-epsilon and -5+epsilon
	static constexpr float topCrossoverLevel = 5.0f - epsilon;
	static constexpr float botCrossoverLevel = -5.0f + epsilon;

	// Need to save, with reset
	bool isVoid[2];
	bool isReverse[2];
	
	// Need to save, no reset
	int panelTheme;
	
	// No need to save, with reset
	int posA, posB;// always between 0 and 7
	bool topCross[2];
	
	// No need to save, no reset
	SchmittTrigger voidTriggers[2];
	SchmittTrigger revTriggers[2];
	float lfoLights[2];

	
	Pulsars() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		// Need to save, no reset
		panelTheme = 0;
		// No need to save, no reset		
		for (int i = 0; i < 2; i++) {
			voidTriggers[i].reset();
			revTriggers[i].reset();
			lfoLights[i] = 0.0f;
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
			topCross[i] = false;
		}
		// No need to save, with reset
		posA = 0;
		posB = 0;
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
		posB = randomu32() % 8;
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
		
		// LFO values
		float lfoVal[2];
		lfoVal[0] = inputs[LFO_INPUTS + 0].value;
		lfoVal[1] = inputs[LFO_INPUTS + 1].value;
		
		
		// Pulsar A
		bool active8[8];
		bool atLeastOneActive = false;
		for (int i = 0; i < 8; i++) {
			active8[i] = inputs[INA_INPUTS + i].active;
			if (active8[i]) 
				atLeastOneActive = true;
		}
		if (atLeastOneActive) {
			if (!isVoid[0] && !active8[posA])// ensure start on valid input when no void
				posA = getClosestActive(posA, active8, false);
			
			int posAnext = (posA + (isReverse[0] ? 7 : 1)) % 8;// void approach by default (grab slot whether active of not)
			if (!isVoid[0])
				posAnext = getClosestActive(posAnext, active8, isReverse[0]);// no problem if only one active, will reach posA
			
			float lfo01 = (lfoVal[0] + 5.0f) / 10.0f;
			float posPercent = topCross[0] ? (1.0f - lfo01) : lfo01;
			float nextPosPercent = 1.0f - posPercent;
			outputs[OUTA_OUTPUT].value = posPercent * inputs[INA_INPUTS + posA].value + nextPosPercent * inputs[INA_INPUTS + posAnext].value;
			for (int i = 0; i < 8; i++)
				lights[MIXA_LIGHTS + i].value = 0.0f + ((i == posA) ? posPercent : 0.0f) + ((i == posAnext) ? nextPosPercent : 0.0f);
			
			// PulsarA crossover (LFO detection)
			if (topCross[0] && lfoVal[0] > topCrossoverLevel) {
				topCross[0] = false;// switch to bottom detection now
				posA = posAnext;
				lfoLights[0] = 1.0f;
			}
			else if (!topCross[0] && lfoVal[0] < botCrossoverLevel) {
				topCross[0] = true;// switch to top detection now
				posA = posAnext;
				lfoLights[0] = 1.0f;
			}
		}
		else {
			outputs[OUTA_OUTPUT].value = 0.0f;
			for (int i = 0; i < 8; i++)
				lights[MIXA_LIGHTS + i].value = 0.0f;
		}


		// Pulsar B
		atLeastOneActive = false;
		for (int i = 0; i < 8; i++) {
			active8[i] = outputs[OUTB_OUTPUTS + i].active;
			if (active8[i]) 
				atLeastOneActive = true;
		}
		if (atLeastOneActive) {
			if (!isVoid[1] && !active8[posB])// ensure start on valid output when no void
				posB = getClosestActive(posB, active8, false);
			
			int posBnext = (posB + (isReverse[1] ? 7 : 1)) % 8;// void approach by default (write to slot whether active of not)
			if (!isVoid[1])
				posBnext = getClosestActive(posBnext, active8, isReverse[1]);// no problem if only one active, will reach posB
			
			float lfo01 = (lfoVal[1] + 5.0f) / 10.0f;
			float posPercent = topCross[1] ? (1.0f - lfo01) : lfo01;
			float nextPosPercent = 1.0f - posPercent;
			for (int i = 0; i < 8; i++) {
				outputs[OUTB_OUTPUTS + i].value = 0.0f + ((i == posB) ? (posPercent * inputs[INB_INPUT].value) : 0.0f) + ((i == posBnext) ? (nextPosPercent * inputs[INB_INPUT].value) : 0.0f);
				lights[MIXB_LIGHTS + i].value = 0.0f + ((i == posB) ? posPercent : 0.0f) + ((i == posBnext) ? nextPosPercent : 0.0f);
			}
			
			// PulsarB crossover (LFO detection)
			if (topCross[1] && lfoVal[1] > topCrossoverLevel) {
				topCross[1] = false;// switch to bottom detection now
				posB = posBnext;
				lfoLights[1] = 1.0f;
			}
			else if (!topCross[1] && lfoVal[1] < botCrossoverLevel) {
				topCross[1] = true;// switch to top detection now
				posB = posBnext;
				lfoLights[1] = 1.0f;
			}
		}
		else {
			for (int i = 0; i < 8; i++) {
				outputs[OUTB_OUTPUTS + i].value = 0.0f;
				lights[MIXB_LIGHTS + i].value = 0.0f;
			}
		}

		
		// Void and Reverse lights
		for (int i = 0; i < 2; i++) {
			lights[VOID_LIGHTS + i].value = isVoid[i] ? 1.0f : 0.0f;
			lights[REV_LIGHTS + i].value = isReverse[i] ? 1.0f : 0.0f;
		}
		
		// LFO lights
		for (int i = 0; i < 2; i++) {
			lights[LFO_LIGHTS + i].value = lfoLights[i];
			lfoLights[i] -= (lfoLights[i] / lightLambda) * (float)engineGetSampleTime();
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
		static constexpr float rowRulerPulsarA = 127.5;
		static constexpr float rowRulerPulsarB = 261.5f;
		float rowRulerLFOlights = (rowRulerPulsarA + rowRulerPulsarB) / 2.0f;
		static constexpr float offsetLFOlightsX = 25.0f;
		static constexpr float radiusLeds = 23.0f;
		static constexpr float radiusJacks = 46.0f;
		static constexpr float offsetLeds = 17.0f;
		static constexpr float offsetJacks = 33.0f;
		static constexpr float offsetLFO = 24.0f;// used also for void and reverse CV input jacks
		static constexpr float offsetLedX = 13.0f;// adds/subs to offsetLFO for void and reverse lights
		static constexpr float offsetButtonX = 26.0f;// adds/subs to offsetLFO for void and reverse buttons
		static constexpr float offsetLedY = 11.0f;// adds/subs to offsetLFO for void and reverse lights
		static constexpr float offsetButtonY = 18.0f;// adds/subs to offsetLFO for void and reverse buttons
		

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
		
		// PulsarA void (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarA - offsetJacks - offsetLFO), Port::INPUT, module, Pulsars::VOID_INPUTS + 0, &module->panelTheme));
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetLedX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetLedY), module, Pulsars::VOID_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetButtonX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetButtonY), module, Pulsars::VOID_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));

		// PulsarA reverse (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarA - offsetJacks - offsetLFO), Port::INPUT, module, Pulsars::REV_INPUTS + 0, &module->panelTheme));
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetLedX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetLedY), module, Pulsars::REV_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetButtonX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetButtonY), module, Pulsars::REV_PARAMS + 0, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// PulsarA LFO input and light
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarA + offsetJacks + offsetLFO), Port::INPUT, module, Pulsars::LFO_INPUTS + 0, &module->panelTheme));
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter - offsetLFOlightsX, rowRulerLFOlights), module, Pulsars::LFO_LIGHTS + 0));
		
		
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
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter, rowRulerPulsarB - radiusLeds), module, Pulsars::MIXB_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarB - offsetLeds), module, Pulsars::MIXB_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + radiusLeds, rowRulerPulsarB), module, Pulsars::MIXB_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarB + offsetLeds), module, Pulsars::MIXB_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter, rowRulerPulsarB + radiusLeds), module, Pulsars::MIXB_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarB + offsetLeds), module, Pulsars::MIXB_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - radiusLeds, rowRulerPulsarB), module, Pulsars::MIXB_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<BlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarB - offsetLeds), module, Pulsars::MIXB_LIGHTS + 7));
		
		// PulsarB void (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarB + offsetJacks + offsetLFO), Port::INPUT, module, Pulsars::VOID_INPUTS + 1, &module->panelTheme));
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetLedX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetLedY), module, Pulsars::VOID_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetButtonX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetButtonY), module, Pulsars::VOID_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));

		// PulsarB reverse (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarB + offsetJacks + offsetLFO), Port::INPUT, module, Pulsars::REV_INPUTS + 1, &module->panelTheme));
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetLedX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetLedY), module, Pulsars::REV_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetButtonX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetButtonY), module, Pulsars::REV_PARAMS + 1, 0.0f, 1.0f, 0.0f, &module->panelTheme));
		
		// PulsarA LFO input and light
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarB - offsetJacks - offsetLFO), Port::INPUT, module, Pulsars::LFO_INPUTS + 1, &module->panelTheme));		
		addChild(createLightCentered<SmallLight<WhiteLight>>(Vec(colRulerCenter + offsetLFOlightsX, rowRulerLFOlights), module, Pulsars::LFO_LIGHTS + 1));
	}
};

Model *modelPulsars = Model::create<Pulsars, PulsarsWidget>("Geodesics", "Pulsars", "Pulsars", MIXER_TAG);

/*CHANGE LOG

0.6.0:
created

*/