//***********************************************************************************************
//Neutron Powered Rotating Crossfader module for VCV Rack by Pierre Collard and Marc Boulé
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
		ENUMS(RND_PARAMS, 2),// push-button
		ENUMS(CVLEVEL_PARAMS, 2),// push-button
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(INA_INPUTS, 8),
		INB_INPUT,
		ENUMS(LFO_INPUTS, 2),
		ENUMS(VOID_INPUTS, 2),
		ENUMS(REV_INPUTS, 2),
		NUM_INPUTS
	};
	enum OutputIds {
		OUTA_OUTPUT,
		ENUMS(OUTB_OUTPUTS, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(LFO_LIGHTS, 2),
		ENUMS(MIXA_LIGHTS, 8),
		ENUMS(MIXB_LIGHTS, 8),
		ENUMS(VOID_LIGHTS, 2),
		ENUMS(REV_LIGHTS, 2),
		ENUMS(RND_LIGHTS, 2),
		ENUMS(CVALEVEL_LIGHTS, 2),// White, but two lights (light 0 is cvMode bit = 0, light 1 is cvMode bit = 1)
		ENUMS(CVBLEVEL_LIGHTS, 2),// White, but two lights
		NUM_LIGHTS
	};
	
	
	// Constants
	static constexpr float epsilon = 0.0001f;// pulsar crossovers at epsilon and 1-epsilon in 0.0f to 1.0f space

	
	// Need to save
	int panelTheme = 0;
	int cvMode;// 0 is -5v to 5v, 1 is 0v to 10v; bit 0 is upper Pulsar, bit 1 is lower Pulsar
	bool isVoid[2];
	bool isReverse[2];
	bool isRandom[2];
	
	
	// No need to save
	bool topCross[2];
	int posA;// always between 0 and 7
	int posB;// always between 0 and 7
	int posAnext;// always between 0 and 7
	int posBnext;// always between 0 and 7
	Trigger voidTriggers[2];
	Trigger revTriggers[2];
	Trigger rndTriggers[2];
	Trigger cvLevelTriggers[2];
	float lfoLights[2] = {0.0f, 0.0f};
	unsigned int lightRefreshCounter = 0;

	
	Pulsars() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(VOID_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top pulsar void");
		configParam(REV_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top pulsar reverse");	
		configParam(RND_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top pulsar random");
		configParam(CVLEVEL_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Top pulsar uni/bi-polar");	
		configParam(VOID_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom pulsar void");
		configParam(REV_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom pulsar reverse");	
		configParam(RND_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom pulsar random");		
		configParam(CVLEVEL_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Bottom pulsar uni/bi-polar");

		onReset();
	}

	
	void onReset() override {
		cvMode = 0;
		for (int i = 0; i < 2; i++) {
			isVoid[i] = false;
			isReverse[i] = false;
			isRandom[i] = false;
			topCross[i] = false;
		}
		posA = 0;// no need to check isVoid here, will be checked in step()
		posB = 0;// no need to check isVoid here, will be checked in step()
		posAnext = 1;// no need to check isVoid here, will be checked in step()
		posBnext = 1;// no need to check isVoid here, will be checked in step()
	}

	
	void onRandomize() override {
		for (int i = 0; i < 2; i++) {
			isVoid[i] = (random::u32() % 2) > 0;
			isReverse[i] = (random::u32() % 2) > 0;
			isRandom[i] = (random::u32() % 2) > 0;
		}
		posA = random::u32() % 8;// no need to check isVoid here, will be checked in step()
		posB = random::u32() % 8;// no need to check isVoid here, will be checked in step()
		posAnext = (posA + (isReverse[0] ? 7 : 1)) % 8;// no need to check isVoid here, will be checked in step()
		posBnext = (posB + (isReverse[1] ? 7 : 1)) % 8;// no need to check isVoid here, will be checked in step()
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// isVoid
		json_object_set_new(rootJ, "isVoid0", json_real(isVoid[0]));
		json_object_set_new(rootJ, "isVoid1", json_real(isVoid[1]));
		
		// isReverse
		json_object_set_new(rootJ, "isReverse0", json_real(isReverse[0]));
		json_object_set_new(rootJ, "isReverse1", json_real(isReverse[1]));
		
		// isRandom
		json_object_set_new(rootJ, "isRandom0", json_real(isRandom[0]));
		json_object_set_new(rootJ, "isRandom1", json_real(isRandom[1]));
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// cvMode
		json_object_set_new(rootJ, "cvMode", json_integer(cvMode));

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// isVoid
		json_t *isVoid0J = json_object_get(rootJ, "isVoid0");
		if (isVoid0J)
			isVoid[0] = json_number_value(isVoid0J);
		json_t *isVoid1J = json_object_get(rootJ, "isVoid1");
		if (isVoid1J)
			isVoid[1] = json_number_value(isVoid1J);

		// isReverse
		json_t *isReverse0J = json_object_get(rootJ, "isReverse0");
		if (isReverse0J)
			isReverse[0] = json_number_value(isReverse0J);
		json_t *isReverse1J = json_object_get(rootJ, "isReverse1");
		if (isReverse1J)
			isReverse[1] = json_number_value(isReverse1J);

		// isRandom
		json_t *isRandom0J = json_object_get(rootJ, "isRandom0");
		if (isRandom0J)
			isRandom[0] = json_number_value(isRandom0J);
		json_t *isRandom1J = json_object_get(rootJ, "isRandom1");
		if (isRandom1J)
			isRandom[1] = json_number_value(isRandom1J);

		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// cvMode
		json_t *cvModeJ = json_object_get(rootJ, "cvMode");
		if (cvModeJ)
			cvMode = json_integer_value(cvModeJ);

		posA = 0;// no need to check isVoid here, will be checked in step()
		posB = 0;// no need to check isVoid here, will be checked in step()
		posAnext = (posA + (isReverse[0] ? 7 : 1)) % 8;// no need to check isVoid here, will be checked in step()
		posBnext = (posB + (isReverse[1] ? 7 : 1)) % 8;// no need to check isVoid here, will be checked in step()
	}

	
	void process(const ProcessArgs &args) override {
		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {

			// Void, Reverse and Random buttons
			for (int i = 0; i < 2; i++) {
				if (voidTriggers[i].process(params[VOID_PARAMS + i].value + inputs[VOID_INPUTS + i].getVoltage())) {
					isVoid[i] = !isVoid[i];
				}
				if (revTriggers[i].process(params[REV_PARAMS + i].value + inputs[REV_INPUTS + i].getVoltage())) {
					isReverse[i] = !isReverse[i];
				}
				if (rndTriggers[i].process(params[RND_PARAMS + i].value)) {// + inputs[RND_INPUTS + i].getVoltage())) {
					isRandom[i] = !isRandom[i];
				}
			}
			
			// CV Level buttons
			for (int i = 0; i < 2; i++) {
				if (cvLevelTriggers[i].process(params[CVLEVEL_PARAMS + i].value))
					cvMode ^= (0x1 << i);
			}
			
		}// userInputs refresh

		// LFO values (normalized to 0.0f to 1.0f space, clamped and offset adjusted depending cvMode)
		float lfoVal[2];
		lfoVal[0] = inputs[LFO_INPUTS + 0].getVoltage();
		lfoVal[1] = inputs[LFO_INPUTS + 1].isConnected() ? inputs[LFO_INPUTS + 1].getVoltage() : lfoVal[0];
		for (int i = 0; i < 2; i++)
			lfoVal[i] = clamp( (lfoVal[i] + ((cvMode & (0x1 << i)) == 0 ? 5.0f : 0.0f)) / 10.0f , 0.0f , 1.0f);
		
		
		// Pulsar A
		bool active8[8];
		bool atLeastOneActive = false;
		for (int i = 0; i < 8; i++) {
			active8[i] = inputs[INA_INPUTS + i].isConnected();
			if (active8[i]) 
				atLeastOneActive = true;
		}
		if (atLeastOneActive) {
			if (!isVoid[0]) {
				if (!active8[posA])// ensure start on valid input when no void
					posA = getNextClosestActive(posA, active8, false, false, false);
				if (!active8[posAnext])
					posAnext = getNextClosestActive(posA, active8, false, isReverse[0], isRandom[0]);
			}			
			
			float posPercent = topCross[0] ? (1.0f - lfoVal[0]) : lfoVal[0];
			float nextPosPercent = 1.0f - posPercent;
			outputs[OUTA_OUTPUT].setVoltage(posPercent * inputs[INA_INPUTS + posA].getVoltage() + nextPosPercent * inputs[INA_INPUTS + posAnext].getVoltage());
			for (int i = 0; i < 8; i++)
				lights[MIXA_LIGHTS + i].setBrightness(0.0f + ((i == posA) ? posPercent : 0.0f) + ((i == posAnext) ? nextPosPercent : 0.0f));
			
			// PulsarA crossover (LFO detection)
			if ( (topCross[0] && lfoVal[0] > (1.0f - epsilon)) || (!topCross[0] && lfoVal[0] < epsilon) ) {
				topCross[0] = !topCross[0];// switch to opposite detection
				posA = posAnext;
				posAnext = getNextClosestActive(posA, active8, isVoid[0], isReverse[0], isRandom[0]);
				lfoLights[0] = 1.0f;
			}
		}
		else {
			outputs[OUTA_OUTPUT].setVoltage(0.0f);
			for (int i = 0; i < 8; i++)
				lights[MIXA_LIGHTS + i].value = 0.0f;
		}


		// Pulsar B
		atLeastOneActive = false;
		for (int i = 0; i < 8; i++) {
			active8[i] = outputs[OUTB_OUTPUTS + i].isConnected();
			if (active8[i]) 
				atLeastOneActive = true;
		}
		if (atLeastOneActive) {
			if (!isVoid[1]) {
				if (!active8[posB])// ensure start on valid output when no void
					posB = getNextClosestActive(posB, active8, false, false, false);
				if (!active8[posBnext])
					posBnext = getNextClosestActive(posB, active8, false, isReverse[1], isRandom[1]);
			}			
			
			float posPercent = topCross[1] ? (1.0f - lfoVal[1]) : lfoVal[1];
			float nextPosPercent = 1.0f - posPercent;
			for (int i = 0; i < 8; i++) {
				if (inputs[INB_INPUT].isConnected())
					outputs[OUTB_OUTPUTS + i].setVoltage(0.0f + ((i == posB) ? (posPercent * inputs[INB_INPUT].getVoltage()) : 0.0f) + ((i == posBnext) ? (nextPosPercent * inputs[INB_INPUT].getVoltage()) : 0.0f));
				else// mutidimentional trick
					outputs[OUTB_OUTPUTS + i].setVoltage(0.0f + ((i == posB) ? (posPercent * inputs[INA_INPUTS + i].getVoltage()) : 0.0f) + ((i == posBnext) ? (nextPosPercent * inputs[INA_INPUTS + i].getVoltage()) : 0.0f));
				lights[MIXB_LIGHTS + i].setBrightness(0.0f + ((i == posB) ? posPercent : 0.0f) + ((i == posBnext) ? nextPosPercent : 0.0f));
			}
			
			// PulsarB crossover (LFO detection)
			if ( (topCross[1] && lfoVal[1] > (1.0f - epsilon)) || (!topCross[1] && lfoVal[1] < epsilon) ) {
				topCross[1] = !topCross[1];// switch to opposite detection
				posB = posBnext;
				posBnext = getNextClosestActive(posB, active8, isVoid[1], isReverse[1], isRandom[1]);
				lfoLights[1] = 1.0f;
			}
		}
		else {
			for (int i = 0; i < 8; i++) {
				outputs[OUTB_OUTPUTS + i].setVoltage(0.0f);
				lights[MIXB_LIGHTS + i].value = 0.0f;
			}
		}

		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;

			// Void, Reverse and Random lights
			for (int i = 0; i < 2; i++) {
				lights[VOID_LIGHTS + i].value = isVoid[i] ? 1.0f : 0.0f;
				lights[REV_LIGHTS + i].value = isReverse[i] ? 1.0f : 0.0f;
				lights[RND_LIGHTS + i].value = isRandom[i] ? 1.0f : 0.0f;
			}
			
			// CV Level lights
			bool isBiolar = (cvMode & 0x1) == 0;
			lights[CVALEVEL_LIGHTS + 0].value = isBiolar ? 1.0f : 0.0f;
			lights[CVALEVEL_LIGHTS + 1].value = isBiolar ? 0.0f : 1.0f;
			isBiolar = (cvMode & 0x2) == 0;
			lights[CVBLEVEL_LIGHTS + 0].value = isBiolar ? 1.0f : 0.0f;
			lights[CVBLEVEL_LIGHTS + 1].value = isBiolar ? 0.0f : 1.0f;

			// LFO lights
			for (int i = 0; i < 2; i++) {
				lights[LFO_LIGHTS + i].value = lfoLights[i];
				lfoLights[i] -= (lfoLights[i] / lightLambda) * (float)args.sampleTime * displayRefreshStepSkips;
			}
			
		}// lightRefreshCounter
		
	}// step()
	
	
	int getNextClosestActive(int pos, bool* active8, bool voidd, bool reverse, bool random) {
		// finds the next closest active position (excluding current if active)
		// assumes at least one active, but may not be given pos; will always return an active pos
		// scans all 8 positions
		int posNext = -1;// should never be returned
		if (random) {
			if (voidd)
				posNext = (pos + 1 + random::u32() % 7) % 8;
			else {
				posNext = pos;
				int activeIndexes[8];// room for all indexes of active positions except current if active(max size is guaranteed to be < 8)
				int activeIndexesI = 0;
				for (int i = 0; i < 8; i++) {
					if (active8[i] && i != pos) {
						activeIndexes[activeIndexesI] = i;
						activeIndexesI++;
					}
				}
				if (activeIndexesI > 0)
					posNext = activeIndexes[random::u32()%activeIndexesI];
			}	
		}
		else { 
			posNext = (pos + (reverse ? 7 : 1)) % 8;// void approach by default (choose slot whether active of not)
			if (!voidd) {
				if (reverse) {
					for (int i = posNext + 8; i > posNext; i--) {
						if (active8[i % 8]) {
							posNext = i % 8;
							break;
						}
					}
				}
				else {
					for (int i = posNext; i < posNext + 8; i++) {
						if (active8[i % 8]) {
							posNext = i % 8;
							break;
						}
					}
				}
			}
		}
		return posNext;
	}
};


struct PulsarsWidget : ModuleWidget {
	SvgPanel* lightPanel;
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		Pulsars *module;
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
	}	
	
	PulsarsWidget(Pulsars *module) {
		setModule(module);

		// Main panels from Inkscape
        lightPanel = new SvgPanel();
        lightPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/Pulsars-WL.svg")));
        box.size = lightPanel->box.size;
        addChild(lightPanel);
        darkPanel = new SvgPanel();
		darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/Pulsars-DM.svg")));
		darkPanel->visible = false;
		addChild(darkPanel);

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
		static constexpr float offsetRndButtonX = 58.0f;// from center of pulsar
		static constexpr float offsetRndButtonY = 24.0f;// from center of pulsar
		static constexpr float offsetRndLedX = 63.0f;// from center of pulsar
		static constexpr float offsetRndLedY = 11.0f;// from center of pulsar
		static constexpr float offsetLedVsButBX = 8.0f;// BI
		static constexpr float offsetLedVsButBY = 10.0f;
		static constexpr float offsetLedVsButUX = 13.0f;// UNI
		static constexpr float offsetLedVsButUY = 1.0f;


		// PulsarA center output
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA), false, module, Pulsars::OUTA_OUTPUT, module ? &module->panelTheme : NULL));
		
		// PulsarA inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA - radiusJacks), true, module, Pulsars::INA_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarA - offsetJacks), true, module, Pulsars::INA_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusJacks, rowRulerPulsarA), true, module, Pulsars::INA_INPUTS + 2, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarA + offsetJacks), true, module, Pulsars::INA_INPUTS + 3, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarA + radiusJacks), true, module, Pulsars::INA_INPUTS + 4, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarA + offsetJacks), true, module, Pulsars::INA_INPUTS + 5, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusJacks, rowRulerPulsarA), true, module, Pulsars::INA_INPUTS + 6, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarA - offsetJacks), true, module, Pulsars::INA_INPUTS + 7, module ? &module->panelTheme : NULL));

		// PulsarA lights
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerPulsarA - radiusLeds), module, Pulsars::MIXA_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarA - offsetLeds), module, Pulsars::MIXA_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + radiusLeds, rowRulerPulsarA), module, Pulsars::MIXA_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarA + offsetLeds), module, Pulsars::MIXA_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerPulsarA + radiusLeds), module, Pulsars::MIXA_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarA + offsetLeds), module, Pulsars::MIXA_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - radiusLeds, rowRulerPulsarA), module, Pulsars::MIXA_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarA - offsetLeds), module, Pulsars::MIXA_LIGHTS + 7));
		
		// PulsarA void (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarA - offsetJacks - offsetLFO), true, module, Pulsars::VOID_INPUTS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetLedX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetLedY), module, Pulsars::VOID_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetButtonX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetButtonY), module, Pulsars::VOID_PARAMS + 0, module ? &module->panelTheme : NULL));

		// PulsarA reverse (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarA - offsetJacks - offsetLFO), true, module, Pulsars::REV_INPUTS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetLedX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetLedY), module, Pulsars::REV_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetButtonX, rowRulerPulsarA - offsetJacks - offsetLFO - offsetButtonY), module, Pulsars::REV_PARAMS + 0, module ? &module->panelTheme : NULL));
		
		// PulsarA random (light and button)
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetRndLedX, rowRulerPulsarA + offsetRndLedY), module, Pulsars::RND_LIGHTS + 0));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetRndButtonX, rowRulerPulsarA + offsetRndButtonY), module, Pulsars::RND_PARAMS + 0, module ? &module->panelTheme : NULL));

		// PulsarA CV level (lights and button)
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetRndButtonX, rowRulerPulsarA + offsetRndButtonY), module, Pulsars::CVLEVEL_PARAMS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetRndButtonX - offsetLedVsButBX, rowRulerPulsarA + offsetRndButtonY + offsetLedVsButBY), module, Pulsars::CVALEVEL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetRndButtonX - offsetLedVsButUX, rowRulerPulsarA + offsetRndButtonY - offsetLedVsButUY), module, Pulsars::CVALEVEL_LIGHTS + 1));

		// PulsarA LFO input and light
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarA + offsetJacks + offsetLFO), true, module, Pulsars::LFO_INPUTS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetLFOlightsX, rowRulerLFOlights), module, Pulsars::LFO_LIGHTS + 0));
		
		
		// PulsarB center input
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB), true, module, Pulsars::INB_INPUT, module ? &module->panelTheme : NULL));
		
		// PulsarB outputs
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB - radiusJacks), false, module, Pulsars::OUTB_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarB - offsetJacks), false, module, Pulsars::OUTB_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + radiusJacks, rowRulerPulsarB), false, module, Pulsars::OUTB_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks, rowRulerPulsarB + offsetJacks), false, module, Pulsars::OUTB_OUTPUTS + 3, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, rowRulerPulsarB + radiusJacks), false, module, Pulsars::OUTB_OUTPUTS + 4, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarB + offsetJacks), false, module, Pulsars::OUTB_OUTPUTS + 5, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - radiusJacks, rowRulerPulsarB), false, module, Pulsars::OUTB_OUTPUTS + 6, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks, rowRulerPulsarB - offsetJacks), false, module, Pulsars::OUTB_OUTPUTS + 7, module ? &module->panelTheme : NULL));

		// PulsarB lights
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerPulsarB - radiusLeds), module, Pulsars::MIXB_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarB - offsetLeds), module, Pulsars::MIXB_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + radiusLeds, rowRulerPulsarB), module, Pulsars::MIXB_LIGHTS + 2));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter + offsetLeds, rowRulerPulsarB + offsetLeds), module, Pulsars::MIXB_LIGHTS + 3));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter, rowRulerPulsarB + radiusLeds), module, Pulsars::MIXB_LIGHTS + 4));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarB + offsetLeds), module, Pulsars::MIXB_LIGHTS + 5));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - radiusLeds, rowRulerPulsarB), module, Pulsars::MIXB_LIGHTS + 6));
		addChild(createLightCentered<SmallLight<GeoBlueLight>>(Vec(colRulerCenter - offsetLeds, rowRulerPulsarB - offsetLeds), module, Pulsars::MIXB_LIGHTS + 7));
		
		// PulsarB void (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetJacks - offsetLFO, rowRulerPulsarB + offsetJacks + offsetLFO), true, module, Pulsars::VOID_INPUTS + 1, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetLedX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetLedY), module, Pulsars::VOID_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetJacks - offsetLFO + offsetButtonX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetButtonY), module, Pulsars::VOID_PARAMS + 1, module ? &module->panelTheme : NULL));

		// PulsarB reverse (jack, light and button)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarB + offsetJacks + offsetLFO), true, module, Pulsars::REV_INPUTS + 1, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetLedX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetLedY), module, Pulsars::REV_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetJacks + offsetLFO - offsetButtonX, rowRulerPulsarB + offsetJacks + offsetLFO + offsetButtonY), module, Pulsars::REV_PARAMS + 1, module ? &module->panelTheme : NULL));
		
		// PulsarB random (light and button)
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetRndLedX, rowRulerPulsarB - offsetRndLedY), module, Pulsars::RND_LIGHTS + 1));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetRndButtonX, rowRulerPulsarB - offsetRndButtonY), module, Pulsars::RND_PARAMS + 1, module ? &module->panelTheme : NULL));
		
		// PulsarB CV level (lights and button)
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetRndButtonX, rowRulerPulsarB - offsetRndButtonY), module, Pulsars::CVLEVEL_PARAMS + 1,  module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetRndButtonX + offsetLedVsButBX, rowRulerPulsarB - offsetRndButtonY - offsetLedVsButBY), module, Pulsars::CVBLEVEL_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetRndButtonX + offsetLedVsButUX, rowRulerPulsarB - offsetRndButtonY + offsetLedVsButUY), module, Pulsars::CVBLEVEL_LIGHTS + 1));

		// PulsarA LFO input and light
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetJacks + offsetLFO, rowRulerPulsarB - offsetJacks - offsetLFO), true, module, Pulsars::LFO_INPUTS + 1, module ? &module->panelTheme : NULL));		
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetLFOlightsX, rowRulerLFOlights), module, Pulsars::LFO_LIGHTS + 1));
	}
	
	void step() override {
		if (module) {
			lightPanel->visible = ((((Pulsars*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Pulsars*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelPulsars = createModel<Pulsars, PulsarsWidget>("Pulsars");

/*CHANGE LOG


*/