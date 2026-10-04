#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include <memory>

class ofApp : public ofBaseApp{

	public:
		enum class Tool {
			Brush,
			Pen,
			Pencil,
			Marker,
			Paintbrush,
			Fill
		};

		struct FillSpan {
			int x;
			int y;
			int width;
		};

		struct Stroke {
			std::vector<ofVec2f> points;
			std::vector<float> pressures;
			std::vector<FillSpan> fillSpans;
			ofColor color;
			float width;
			Tool tool;
			bool eraser;
		};

		struct Layer {
			std::string name;
			std::vector<Stroke> strokes;
			ofFbo image;
			bool visible = true;
			float opacity = 1.0f;
		};

		void setup();
		void exit();
		void update();
		void draw();
		void resizeCanvas(int width, int height);
		void drawStroke(const Stroke &stroke);
		void drawStrokeLayer(const Stroke &stroke);
		void renderStroke(const Stroke &stroke);
		void applyStroke(Layer &layer, const Stroke &stroke);
		void rebuildLayer(Layer &layer);
		void rebuildCanvas();
				void detectFillPixelOrientation();
		Layer &activeLayer();
		void addLayer();
		void removeActiveLayer();
		void moveActiveLayer(int direction);
		void drawLayerPanel();
		void updateLayerPanelLayout();
		bool handleLayerPanelPress(int x, int y);
		void setActiveLayerOpacity(int x);
		void fillAt(int x, int y);
		void setupTabletPressureInput();
		void pollTabletPressure();
		void appendStrokePoint(const ofVec2f &point, float pressure);
		void undoLastStroke();
		void clearCanvas();
		void saveArtwork();
		void finishActiveStroke();
		void selectTool(Tool tool);
		void brushToolChanged(bool &enabled);
		void penToolChanged(bool &enabled);
		void pencilToolChanged(bool &enabled);
		void markerToolChanged(bool &enabled);
		void paintbrushToolChanged(bool &enabled);
		void fillToolChanged(bool &enabled);

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

	private:
		struct TabletPressureAxis {
			int deviceId;
			int axis;
			double minimum;
			double maximum;
		};

		std::vector<std::unique_ptr<Layer>> layers;
		Stroke activeStroke;
		bool drawingStroke = false;
		bool pendingStroke = false;
		int pendingMotionSamples = 0;
		uint64_t pendingStrokeStartedAt = 0;
		bool draggingLayerOpacity = false;
		bool fillReadbackFlipped = false;
		void *tabletDisplay = nullptr;
		int tabletEventOpcode = -1;
		int tabletPressureDeviceId = -1;
		bool tabletPressureSourceActive = false;
		float tabletPressure = 1.0f;
		bool tabletPointerPositionValid = false;
		bool tabletStrokeInput = false;
		bool tabletStrokePositionInitialized = false;
		ofVec2f tabletPointerPosition;
		std::vector<TabletPressureAxis> tabletPressureAxes;
		std::vector<int> auxiliaryTabletDeviceIds;
		ofFbo canvas;
		ofFbo strokeLayer;
		ofFbo exportBuffer;
		ofRectangle canvasBounds;
		ofRectangle layerPanelBounds;
		ofRectangle layerAddBounds;
		ofRectangle layerRemoveBounds;
		ofRectangle layerUpBounds;
		ofRectangle layerDownBounds;
		ofRectangle layerOpacityBounds;
		std::vector<ofRectangle> layerRowBounds;
		std::vector<ofRectangle> layerVisibilityBounds;
		ofxPanel gui;
		ofParameter<ofColor> brushColor;
		ofParameter<bool> brushTool;
		ofParameter<bool> penTool;
		ofParameter<bool> pencilTool;
		ofParameter<bool> markerTool;
		ofParameter<bool> paintbrushTool;
		ofParameter<bool> fillTool;
		ofParameter<float> brushSize;
		ofParameter<int> brushOpacity;
		ofParameter<int> fillTolerance;
		ofParameter<bool> eraser;
		ofParameter<void> undoAction;
		ofParameter<void> clearAction;
		ofParameter<void> saveAction;
		ofColor backgroundColor;
		ofColor paperColor;
		Tool selectedTool = Tool::Brush;
		std::size_t activeLayerIndex = 0;
		int nextLayerNumber = 0;
		
};
