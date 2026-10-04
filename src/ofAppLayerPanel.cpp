#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::updateLayerPanelLayout(){
	const float panelWidth = 194.0f;
	const float panelX = std::max(8.0f, ofGetWidth() - panelWidth - 18.0f);
	const float rowsY = 84.0f;
	const float rowHeight = 23.0f;
	layerPanelBounds.set(panelX, 16, panelWidth,
		rowsY + layers.size() * rowHeight + 52.0f);
	layerAddBounds.set(panelX + 8, 50, 36, 24);
	layerRemoveBounds.set(panelX + 48, 50, 36, 24);
	layerUpBounds.set(panelX + 88, 50, 36, 24);
	layerDownBounds.set(panelX + 128, 50, 36, 24);
	layerRowBounds.clear();
	layerVisibilityBounds.clear();
	for (std::size_t row = 0; row < layers.size(); ++row) {
		const float rowY = rowsY + row * rowHeight;
		layerRowBounds.emplace_back(panelX + 8, rowY, panelWidth - 16, rowHeight - 2);
		layerVisibilityBounds.emplace_back(panelX + panelWidth - 38, rowY, 28, rowHeight - 2);
	}
	layerOpacityBounds.set(panelX + 12, rowsY + layers.size() * rowHeight + 22,
		panelWidth - 24, 8);
}

//--------------------------------------------------------------
void ofApp::drawLayerPanel(){
	updateLayerPanelLayout();
	ofPushStyle();
	ofSetColor(8, 15, 20, 238);
	ofDrawRectangle(layerPanelBounds);
	ofSetColor(28, 130, 142);
	ofDrawRectangle(layerPanelBounds.x, layerPanelBounds.y, layerPanelBounds.width, 28);
	ofSetColor(235, 242, 240);
	ofDrawBitmapString("LAYERS", layerPanelBounds.x + 10, layerPanelBounds.y + 19);

	auto drawButton = [](const ofRectangle &bounds, const std::string &label){
		ofSetColor(35, 57, 62);
		ofDrawRectangle(bounds);
		ofSetColor(230, 238, 236);
		ofDrawBitmapString(label, bounds.x + 10, bounds.y + 17);
	};
	drawButton(layerAddBounds, "+");
	drawButton(layerRemoveBounds, "-");
	drawButton(layerUpBounds, "UP");
	drawButton(layerDownBounds, "DN");

	for (std::size_t row = 0; row < layers.size(); ++row) {
		const std::size_t index = layers.size() - row - 1;
		const Layer &layer = *layers[index];
		ofSetColor(index == activeLayerIndex ? ofColor(35, 112, 115) : ofColor(20, 33, 38));
		ofDrawRectangle(layerRowBounds[row]);
		ofSetColor(230, 238, 236);
		ofDrawBitmapString(layer.name, layerRowBounds[row].x + 7, layerRowBounds[row].y + 15);
		ofSetColor(layer.visible ? ofColor(55, 135, 126) : ofColor(70, 75, 76));
		ofDrawRectangle(layerVisibilityBounds[row]);
		ofSetColor(245);
		ofDrawBitmapString(layer.visible ? "ON" : "OFF",
			layerVisibilityBounds[row].x + 3, layerVisibilityBounds[row].y + 15);
	}

	const int opacityPercent = static_cast<int>(activeLayer().opacity * 100.0f);
	ofSetColor(220, 230, 232);
	ofDrawBitmapString("Opacity " + ofToString(opacityPercent) + "%",
		layerPanelBounds.x + 10, layerOpacityBounds.y - 7);
	ofSetColor(44, 60, 64);
	ofDrawRectangle(layerOpacityBounds);
	ofSetColor(39, 164, 157);
	ofDrawRectangle(layerOpacityBounds.x, layerOpacityBounds.y,
		layerOpacityBounds.width * activeLayer().opacity, layerOpacityBounds.height);
	ofPopStyle();
}

//--------------------------------------------------------------
void ofApp::setActiveLayerOpacity(int x){
	activeLayer().opacity = ofClamp((x - layerOpacityBounds.x) / layerOpacityBounds.width, 0.0f, 1.0f);
	rebuildCanvas();
}

//--------------------------------------------------------------
bool ofApp::handleLayerPanelPress(int x, int y){
	updateLayerPanelLayout();
	if (!layerPanelBounds.inside(x, y)) {
		return false;
	}
	if (layerAddBounds.inside(x, y)) {
		addLayer();
	} else if (layerRemoveBounds.inside(x, y)) {
		removeActiveLayer();
	} else if (layerUpBounds.inside(x, y)) {
		moveActiveLayer(1);
	} else if (layerDownBounds.inside(x, y)) {
		moveActiveLayer(-1);
	} else if (layerOpacityBounds.inside(x, y)) {
		draggingLayerOpacity = true;
		setActiveLayerOpacity(x);
	} else {
		for (std::size_t row = 0; row < layerRowBounds.size(); ++row) {
			const std::size_t index = layers.size() - row - 1;
			if (layerVisibilityBounds[row].inside(x, y)) {
				layers[index]->visible = !layers[index]->visible;
				rebuildCanvas();
				break;
			}
			if (layerRowBounds[row].inside(x, y)) {
				activeLayerIndex = index;
				break;
			}
		}
	}
	return true;
}
