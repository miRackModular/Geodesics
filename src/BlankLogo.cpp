//***********************************************************************************************
//Blank-Panel Logo for VCV Rack by Pierre Collard and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "Geodesics.hpp"


// From Fundamental LFO.cpp
struct LowFrequencyOscillator {
	float phase = 0.0f;
	float freq = 1.0f;

	LowFrequencyOscillator() {}
	void setPitch(float pitch) {
		pitch = fminf(pitch, 8.0f);
		freq = powf(2.0f, pitch);
	}
	void step(float dt) {
		float deltaPhase = fminf(freq * dt, 0.5f);
		phase += deltaPhase;
		if (phase >= 1.0f)
			phase -= 1.0f;
	}
	float sqr() {
		return (phase < 0.5f) ? 2.0f : 0.0f;
	}
};


//*****************************************************************************


struct BlankLogo : Module {
	enum ParamIds {
		CLK_FREQ_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	
	int panelTheme = 0;
	float clkValue;
	int stepIndex;
	float song[5] = {7.0f/12.0f, 9.0f/12.0f, 5.0f/12.0f, 5.0f/12.0f - 1.0f, 0.0f/12.0f};

	LowFrequencyOscillator oscillatorClk;
	Trigger clkTrigger;
	
	
	BlankLogo() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(CLK_FREQ_PARAM, -2.0f, 4.0f, 1.0f, "CLK freq", " BPM", 2.0f, 60.0f);// 120 BMP when default value  (120 = 60*2^1) diplay params 

		clkTrigger.reset();
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	void onReset() override {	
		clkValue = 0.0f;
		stepIndex = 0;
	}

	void onRandomize() override {
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);
	}

	
	// Advances the module by 1 audio frame with duration 1.0 / engineGetSampleRate()
	void process(const ProcessArgs &args) override {
		if (outputs[OUT_OUTPUT].isConnected()) {
			// CLK
			oscillatorClk.setPitch(params[CLK_FREQ_PARAM].getValue());
			oscillatorClk.step(args.sampleTime);
			float clkValue = oscillatorClk.sqr();	
			
			if (clkTrigger.process(clkValue)) {
				stepIndex++;
				if (stepIndex >= 5)
					stepIndex = 0;
				outputs[OUT_OUTPUT].setVoltage(song[stepIndex]);
			}
		}
	}
};


struct BlankLogoWidget : ModuleWidget {
	SvgPanel* lightPanel;
	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		BlankLogo *module;
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

		BlankLogo *module = dynamic_cast<BlankLogo*>(this->module);
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

	BlankLogoWidget(BlankLogo *module) {
		setModule(module);

		// Main panels from Inkscape
        lightPanel = new SvgPanel();
        lightPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WhiteLight/BlankLogo-WL.svg")));
        box.size = lightPanel->box.size;
        addChild(lightPanel);
        darkPanel = new SvgPanel();
		darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/DarkMatter/BlankLogo-DM.svg")));
		darkPanel->visible = false;
		addChild(darkPanel);

		// Screws
		// part of svg panel, no code required
		
		addParam(createDynamicParam<BlankCKnob>(Vec(29.5f,74.2f), module, BlankLogo::CLK_FREQ_PARAM, module ? &module->panelTheme : NULL));// 120 BMP when default value
		addOutput(createOutputCentered<BlankPort>(Vec(29.5f,187.5f), module, BlankLogo::OUT_OUTPUT));	
	}
	
	void step() override {
		if (module) {
			lightPanel->visible = ((((BlankLogo*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((BlankLogo*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelBlankLogo = createModel<BlankLogo, BlankLogoWidget>("Blank-Panel Logo");
