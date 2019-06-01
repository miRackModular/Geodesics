//***********************************************************************************************
//Relativistic Oscillator module for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//Also based on the BogAudio FM-OP oscillator by Matt Demanett
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//***********************************************************************************************


#include "Geodesics.hpp"
#include "EnergyOsc.hpp"


struct Energy : Module {
	enum ParamIds {
		ENUMS(PLANCK_PARAMS, 2),// push buttons
		ENUMS(MODTYPE_PARAMS, 2),// push buttons
		ROUTING_PARAM,// push button
		ENUMS(FREQ_PARAMS, 2),// rotary knobs (middle)
		ENUMS(MOMENTUM_PARAMS, 2),// rotary knobs (top)
		CROSS_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(FREQCV_INPUTS, 2), // respectively "mass" and "speed of light"
		FREQCV_INPUT, // main voct input
		MULTIPLY_INPUT,
		ENUMS(MOMENTUM_INPUTS, 2),
		NUM_INPUTS
	};
	enum OutputIds {
		ENERGY_OUTPUT,// main output
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(PLANCK_LIGHTS, 2 * 2), // room for two white/blue leds
		ENUMS(ADD_LIGHTS, 2),
		ENUMS(AMP_LIGHTS, 2),
		ENUMS(ROUTING_LIGHTS, 3),
		ENUMS(MOMENTUM_LIGHTS, 2),
		ENUMS(FREQ_ROUTING_LIGHTS, 2 * 2),// room for blue/yellow
		CROSS_LIGHT,
		NUM_LIGHTS
	};
	
	
	// Constants
	// none
	
	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	FMOp* oscM;
	FMOp* oscC;
	int routing;// routing of knob 1. 
		// 0 is independant (i.e. blue only) (bottom light, light index 0),
		// 1 is control (i.e. blue and yellow) (top light, light index 1),
		// 2 is spread (i.e. blue and inv yellow) (middle, light index 2)
	int plancks[2];// index is left/right, value is: 0 = not quantized, 1 = semitones, 2 = 5th+octs
	int modtypes[2];// index is left/right, value is: {0 to 3} = {bypass, add, amp}
	int cross;// cross momentum active or not
	
	// No need to save, with reset
	// none
	
	// No need to save, no reset
	RefreshCounter refresh;
	float feedbacks[2] = {0.0f, 0.0f};
	Trigger routingTrigger;
	Trigger planckTriggers[2];
	Trigger modtypeTriggers[2];
	Trigger crossTrigger;
	SlewLimiter multiplySlew;
	
	
	Energy() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(Energy::CROSS_PARAM, 0.0f, 1.0f, 0.0f, "Momentum crossing");		
		configParam(Energy::MOMENTUM_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Momentum M");
		configParam(Energy::MOMENTUM_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Momentum C");
		configParam(Energy::FREQ_PARAMS + 0, -3.0f, 3.0f, 0.0f, "Freq M");
		configParam(Energy::FREQ_PARAMS + 1, -3.0f, 3.0f, 0.0f, "Freq M");
		configParam(Energy::ROUTING_PARAM, 0.0f, 1.0f, 0.0f, "Routing");
		configParam(Energy::PLANCK_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Quantize (Planck) M");
		configParam(Energy::PLANCK_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Quantize (Planck) C");
		configParam(Energy::MODTYPE_PARAMS + 0, 0.0f, 1.0f, 0.0f, "CV mod type M");
		configParam(Energy::MODTYPE_PARAMS + 1, 0.0f, 1.0f, 0.0f, "CV mod type C");		
		
		oscM = new FMOp(APP->engine->getSampleRate());
		oscC = new FMOp(APP->engine->getSampleRate());
		onSampleRateChange();
		onReset();

		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}
	
	
	~Energy() {
		delete oscM;
		delete oscC;
	}

	
	void onReset() override {
		oscM->onReset();
		oscC->onReset();
		routing = 1;// default is control (i.e. blue and yellow) (top light, light index 1),
		for (int i = 0; i < 2; i++) {
			plancks[i] = 0;
			modtypes[i] = 1;// default is add mode
		}
		cross = 0;
		// resetNonJson();
	}
	// void resetNonJson() {
		// none
	// }	

	
	void onRandomize() override {
	}
	

	void onSampleRateChange() override {
		float sampleRate = APP->engine->getSampleRate();
		oscM->onSampleRateChange(sampleRate);
		oscC->onSampleRateChange(sampleRate);
		multiplySlew.setParams2(sampleRate, 2.5f, 20.0f, 1.0f);
	}
	
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// oscM and oscC
		oscM->dataToJson(rootJ, "oscM_");
		oscC->dataToJson(rootJ, "oscC_");

		// routing
		json_object_set_new(rootJ, "routing", json_integer(routing));

		// plancks
		json_object_set_new(rootJ, "planck0", json_integer(plancks[0]));
		json_object_set_new(rootJ, "planck1", json_integer(plancks[1]));

		// modtypes
		json_object_set_new(rootJ, "modtype0", json_integer(modtypes[0]));
		json_object_set_new(rootJ, "modtype1", json_integer(modtypes[1]));
		
		// cross
		json_object_set_new(rootJ, "cross", json_integer(cross));

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// oscM and oscC
		oscM->dataFromJson(rootJ, "oscM_");
		oscC->dataFromJson(rootJ, "oscC_");

		// routing
		json_t *routingJ = json_object_get(rootJ, "routing");
		if (routingJ)
			routing = json_integer_value(routingJ);

		// plancks
		json_t *planck0J = json_object_get(rootJ, "planck0");
		if (planck0J)
			plancks[0] = json_integer_value(planck0J);
		json_t *planck1J = json_object_get(rootJ, "planck1");
		if (planck1J)
			plancks[1] = json_integer_value(planck1J);

		// modtypes
		json_t *modtype0J = json_object_get(rootJ, "modtype0");
		if (modtype0J)
			modtypes[0] = json_integer_value(modtype0J);
		json_t *modtype1J = json_object_get(rootJ, "modtype1");
		if (modtype1J)
			modtypes[1] = json_integer_value(modtype1J);

		// cross
		json_t *crossJ = json_object_get(rootJ, "cross");
		if (crossJ)
			cross = json_integer_value(crossJ);
		
		// resetNonJson();
	}

	void process(const ProcessArgs &args) override {	
		// user inputs
		if (refresh.processInputs()) {
			// routing
			if (routingTrigger.process(params[ROUTING_PARAM].getValue())) {
				if (++routing > 2)
					routing = 0;
			}
			
			// plancks
			for (int i = 0; i < 2; i++) {
				if (planckTriggers[i].process(params[PLANCK_PARAMS + i].getValue())) {
					if (++plancks[i] > 2)
						plancks[i] = 0;
				}
			}
			
			// modtypes
			for (int i = 0; i < 2; i++) {
				if (modtypeTriggers[i].process(params[MODTYPE_PARAMS + i].getValue())) {
					if (++modtypes[i] > 2)
						modtypes[i] = 0;
				}
			}
			
			// cross
			if (crossTrigger.process(params[CROSS_PARAM].getValue())) {
				if (++cross > 1)
					cross = 0;
			}
		}// userInputs refresh
		
		
		// main signal flow
		// ----------------
		
		float freqKnobs[2] = {calcFreqKnob(0), calcFreqKnob(1)};
		float modSignals[2] = {calcModSignal(0, freqKnobs[0]), calcModSignal(1, freqKnobs[1])};
		if (routing == 1)
			modSignals[1] += modSignals[0];
		else if (routing == 2)
			modSignals[1] -= modSignals[0];
		
		// two values to send to oscs: voct and feedback (aka momentum)
		// voct
		float vocts[2] = {modSignals[0] + inputs[FREQCV_INPUT].getVoltage(), modSignals[1] + inputs[FREQCV_INPUT].getVoltage()};
		// feedback (momentum)
		calcFeedbacks();
		
		// oscillators
		float oscMout = oscM->step(vocts[0], feedbacks[0] * 0.3f);
		float oscCout = oscC->step(vocts[1], feedbacks[1] * 0.3f);
		
		// final attenuverters
		float multVal = multiplySlew.next(inputs[MULTIPLY_INPUT].isConnected() ? (clamp(inputs[MULTIPLY_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f)) : 1.0f);
		float attv1 = oscCout * (oscCout * 0.2f * multVal);
		float attv2 = attv1 * (oscMout / 5.0f);
		
		// output
		outputs[ENERGY_OUTPUT].setVoltage(attv2);

		// lights
		if (refresh.processLights()) {
			// routing
			for (int i = 0; i < 3; i++)
				lights[ROUTING_LIGHTS + i].setBrightness(routing == i ? 1.0f : 0.0f);
			
			for (int i = 0; i < 2; i++) {
				// plancks
				lights[PLANCK_LIGHTS + i * 2 + 0].setBrightness(plancks[i] == 1 ? 1.0f : 0.0f);
				lights[PLANCK_LIGHTS + i * 2 + 1].setBrightness(plancks[i] == 2 ? 1.0f : 0.0f);	
				
				// modtypes
				lights[ADD_LIGHTS + i].setBrightness(modtypes[i] == 1 ? 1.0f : 0.0f);
				lights[AMP_LIGHTS + i].setBrightness(modtypes[i] == 2 ? 1.0f : 0.0f);
				
				// momentum (cross)
				lights[MOMENTUM_LIGHTS + i].setBrightness(feedbacks[i]);

				// momentum (cross)
				float modSignalLight = modSignals[i] / 3.0f;
				lights[FREQ_ROUTING_LIGHTS + 2 * i + 0].setBrightness(modSignalLight);// blue diode
				lights[FREQ_ROUTING_LIGHTS + 2 * i + 1].setBrightness(-modSignalLight);// yellow diode
			}
			
			// cross
			lights[CROSS_LIGHT].setBrightness(cross == 1 ? 1.0f : 0.0f);

		}// lightRefreshCounter
		
	}// step()
	
	inline float calcFreqKnob(int i) {
		if (plancks[i] == 0)// off (smooth)
			return params[FREQ_PARAMS + i].getValue();
		if (plancks[i] == 1)// semitones
			return std::round(params[FREQ_PARAMS + i].getValue() * 12.0f) / 12.0f;
		// 5ths and octs
		int retcv = (int)std::round((params[FREQ_PARAMS + i].getValue() + 3.0f) * 2.0f);
		if ((retcv & 0x1) != 0)
			return (float)(retcv)/2.0f - 3.0f + 0.08333333333f;
		return (float)(retcv)/2.0f - 3.0f;
	}
	
	inline float calcModSignal(int i, float freqValue) {
		if (modtypes[i] == 0 || !inputs[FREQCV_INPUTS + i].isConnected())// bypass
			return freqValue;
		if (modtypes[i] == 1) // add
			return freqValue + inputs[FREQCV_INPUTS + i].getVoltage();
		// amp
		return freqValue * (clamp(inputs[FREQCV_INPUTS + i].getVoltage(), 0.0f, 10.0f) / 10.0f);
	}
	
	inline void calcFeedbacks() {
		const float moIn0 = inputs[MOMENTUM_INPUTS + 0].getVoltage();
		const float moIn1 = inputs[MOMENTUM_INPUTS + 1].getVoltage();
		
		feedbacks[0] = params[MOMENTUM_PARAMS + 0].getValue();
		feedbacks[1] = params[MOMENTUM_PARAMS + 1].getValue();
		if (cross == 0) {
			feedbacks[0] += moIn0 * 0.1f;
			feedbacks[1] += moIn1 * 0.1f;
		}
		else {// cross momentum
			if (moIn0 > 0)
				feedbacks[0] += moIn0 * 0.2f;
			else 
				feedbacks[1] += moIn0 * -0.2f;
			if (moIn1 > 0)
				feedbacks[1] += moIn1 * 0.2f;
			else 
				feedbacks[0] += moIn1 * -0.2f;
		}
		feedbacks[0] = clamp(feedbacks[0], 0.0f, 1.0f);
		feedbacks[1] = clamp(feedbacks[1], 0.0f, 1.0f);
	}
};


struct EnergyWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		Energy *module;
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

		Energy *module = dynamic_cast<Energy*>(this->module);
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
	
	EnergyWidget(Energy *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/Energy-WL.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/Energy-DM.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}

		// Screws 
		// part of svg panel, no code required
		
		float colRulerCenter = box.size.x / 2.0f;

		// main output
		addOutput(createDynamicPort<GeoPort>(Vec(colRulerCenter, 380.0f - 332.5f), false, module, Energy::ENERGY_OUTPUT, module ? &module->panelTheme : NULL));

		// multiply input
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, 380.0f - 280.5f), true, module, Energy::MULTIPLY_INPUT, module ? &module->panelTheme : NULL));
		
		// momentum inputs
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, 380.0f - 236.5f), true, module, Energy::MOMENTUM_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, 380.0f - 181.5f), true, module, Energy::MOMENTUM_INPUTS + 0, module ? &module->panelTheme : NULL));
		
		// cross button and light
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter, 380.0f - 205.5f), module, Energy::CROSS_PARAM, module ? &module->panelTheme : NULL));		
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - 7.5f, 380.0f - 219.5f), module, Energy::CROSS_LIGHT));
		
		// routing lights
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(39, 380.0f - 141.5f), module, Energy::ROUTING_LIGHTS + 0));// bottom
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(51, 380.0f - 154.5f), module, Energy::ROUTING_LIGHTS + 1));// top
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(45, 380.0f - 148.5f), module, Energy::ROUTING_LIGHTS + 2));// middle
		
		// momentum knobs
		static const float offsetX = 30.0f;
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offsetX, 380 - 209), module, Energy::MOMENTUM_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offsetX, 380 - 209), module, Energy::MOMENTUM_PARAMS + 1, module ? &module->panelTheme : NULL));
		
		// momentum lights
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - offsetX, 380.0f - 186.0f), module, Energy::MOMENTUM_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + offsetX, 380.0f - 186.0f), module, Energy::MOMENTUM_LIGHTS + 1));

		// freq routing lights (below momentum lights)
		addChild(createLightCentered<SmallLight<GeoBlueYellowLight>>(Vec(colRulerCenter - offsetX, 380.0f - 177.0f), module, Energy::FREQ_ROUTING_LIGHTS + 2 * 0));
		addChild(createLightCentered<SmallLight<GeoBlueYellowLight>>(Vec(colRulerCenter + offsetX, 380.0f - 177.0f), module, Energy::FREQ_ROUTING_LIGHTS + 2 * 1));

		// freq knobs
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter - offsetX, 380 - 126), module, Energy::FREQ_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoKnob>(Vec(colRulerCenter + offsetX, 380 - 126), module, Energy::FREQ_PARAMS + 1, module ? &module->panelTheme : NULL));
		
		// routing button
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter, 380.0f - 113.5f), module, Energy::ROUTING_PARAM, module ? &module->panelTheme : NULL));
		
		// freq input (v/oct)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter, 380 - 84), true, module, Energy::FREQCV_INPUT, module ? &module->panelTheme : NULL));

		// planck buttons
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetX - 0.5f, 380.0f - 83.5f), module, Energy::PLANCK_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetX + 0.5f, 380.0f - 83.5f), module, Energy::PLANCK_PARAMS + 1, module ? &module->panelTheme : NULL));
		
		// planck lights
		addChild(createLightCentered<SmallLight<GeoWhiteBlueLight>>(Vec(colRulerCenter - offsetX - 0.5f, 380.0f - 97.5f), module, Energy::PLANCK_LIGHTS + 0 * 2));
		addChild(createLightCentered<SmallLight<GeoWhiteBlueLight>>(Vec(colRulerCenter + offsetX + 0.5f, 380.0f - 97.5f), module, Energy::PLANCK_LIGHTS + 1 * 2));
		
		// mod type buttons
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter - offsetX - 0.5f, 380.0f - 57.5f), module, Energy::MODTYPE_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<GeoPushButton>(Vec(colRulerCenter + offsetX + 0.5f, 380.0f - 57.5f), module, Energy::MODTYPE_PARAMS + 1, module ? &module->panelTheme : NULL));
		
		// mod type lights
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - 17.5f, 380.0f - 62.5f), module, Energy::ADD_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + 17.5f, 380.0f - 62.5f), module, Energy::ADD_LIGHTS + 1));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter - 41.5f, 380.0f - 47.5f), module, Energy::AMP_LIGHTS + 0));
		addChild(createLightCentered<SmallLight<GeoWhiteLight>>(Vec(colRulerCenter + 41.5f, 380.0f - 47.5f), module, Energy::AMP_LIGHTS + 1));
		
		// freq inputs (mass and speed of light)
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter - offsetX - 0.5f, 380.0f - 32.5f), true, module, Energy::FREQCV_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<GeoPort>(Vec(colRulerCenter + offsetX + 0.5f, 380.0f - 32.5f), true, module, Energy::FREQCV_INPUTS + 1, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((Energy*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Energy*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelEnergy = createModel<Energy, EnergyWidget>("Energy");

/*CHANGE LOG

*/