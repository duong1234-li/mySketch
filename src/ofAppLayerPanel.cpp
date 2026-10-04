#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::updateLayerPanelLayout(){
	const float panelWidth = 226.0f;
	float panelX = layerPanelBounds.x;
	float panelY = layerPanelBounds.y;
	panelX = layersPanel.getPosition().x;
	panelY = layersPanel.getPosition().y;
	const float rowsY = panelY + 68.0f;
	const float rowHeight = 22.0f;
	layerNewProjectBounds.set(panelX + 8, panelY + 34, 52, 24);
	layerAddBounds.set(panelX + 64, panelY + 34, 24, 24);
	layerRemoveBounds.set(panelX + 90, panelY + 34, 24, 24);
	layerUpBounds.set(panelX + 116, panelY + 34, 24, 24);
	layerDownBounds.set(panelX + 142, panelY + 34, 24, 24);
	layerAddImageBounds.set(panelX + 168, panelY + 34, 24, 24);
	layerRemoveImageBounds.set(panelX + 194, panelY + 34, 24, 24);

	layerPanelRows.clear();
	layerVisibilityBounds.assign(layers.size(), ofRectangle());
	for (std::size_t index = layers.size(); index > 0; --index) {
		const std::size_t layerIndex = index - 1;
		layerPanelRows.push_back({LayerItemType::Layer, layerIndex, ofRectangle()});
		if (layers[layerIndex]->hasImage) {
			layerPanelRows.push_back({LayerItemType::Image, layerIndex, ofRectangle()});
		}
	}

	const float desiredHeight = std::max(145.0f,
		rowsY + layerPanelRows.size() * rowHeight + 42.0f - 16.0f);
	const float panelHeight = std::min(desiredHeight,
		std::max(120.0f, ofGetHeight() - 32.0f));
	layerPanelBounds.set(panelX, panelY, panelWidth, panelHeight);
	const float opacityY = layerPanelBounds.y + panelHeight - 20.0f;
	layerPanelListBounds.set(panelX + 4, rowsY, panelWidth - 8,
		std::max(0.0f, opacityY - 14.0f - rowsY));
	const float contentHeight = layerPanelRows.size() * rowHeight;
	layerPanelScrollOffset = ofClamp(layerPanelScrollOffset, 0.0f,
		std::max(0.0f, contentHeight - layerPanelListBounds.height));
	layerPanelScrollOffset = std::round(layerPanelScrollOffset / rowHeight) * rowHeight;
	for (std::size_t row = 0; row < layerPanelRows.size(); ++row) {
		const float rowY = rowsY + row * rowHeight - layerPanelScrollOffset;
		layerPanelRows[row].bounds.set(panelX + 8, rowY, panelWidth - 16, rowHeight - 1);
		if (layerPanelRows[row].type == LayerItemType::Layer) {
			layerVisibilityBounds[layerPanelRows[row].layerIndex].set(
				panelX + panelWidth - 38, rowY, 28, rowHeight - 1);
		}
	}
	layerOpacityBounds.set(panelX + 12, opacityY, panelWidth - 24, 8);
}

//--------------------------------------------------------------
void ofApp::drawLayerPanel(){
	updateLayerPanelLayout();
	ofPushStyle();

	auto drawButton = [](const ofRectangle &bounds, const std::string &label){
		ofSetColor(35, 57, 62);
		ofDrawRectangle(bounds);
		ofSetColor(230, 238, 236);
		ofDrawBitmapString(label, bounds.x + 10, bounds.y + 17);
	};
	drawButton(layerNewProjectBounds, "NEW");
	drawButton(layerAddBounds, "+");
	drawButton(layerRemoveBounds, "-");
	drawButton(layerUpBounds, "^");
	drawButton(layerDownBounds, "v");
	drawButton(layerAddImageBounds, "I");
	drawButton(layerRemoveImageBounds, "X");

	for (const auto &row : layerPanelRows) {
		if (row.bounds.y < layerPanelListBounds.y ||
			row.bounds.y + row.bounds.height > layerPanelListBounds.y + layerPanelListBounds.height) {
			continue;
		}
		const Layer &layer = *layers[row.layerIndex];
		const bool isLayerRow = row.type == LayerItemType::Layer;
		const bool isSelected = row.layerIndex == activeLayerIndex && row.type == selectedLayerItemType;
		ofSetColor(isSelected ? ofColor(35, 112, 115) :
			(isLayerRow && row.layerIndex == activeLayerIndex ? ofColor(27, 55, 58) : ofColor(20, 33, 38)));
		ofDrawRectangle(row.bounds);
		ofSetColor(230, 238, 236);
		const std::string itemName = row.type == LayerItemType::Layer ? layer.name : "Image";
		ofDrawBitmapString(itemName, row.bounds.x + (isLayerRow ? 7 : 20), row.bounds.y + 15);
		if (isLayerRow) {
			ofSetColor(layer.visible ? ofColor(55, 135, 126) : ofColor(70, 75, 76));
			ofDrawRectangle(layerVisibilityBounds[row.layerIndex]);
			ofSetColor(245);
			ofDrawBitmapString(layer.visible ? "ON" : "OFF",
				layerVisibilityBounds[row.layerIndex].x + 3, row.bounds.y + 15);
		}
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
	if (layerNewProjectBounds.inside(x, y)) {
		newProject();
	} else if (layerAddBounds.inside(x, y)) {
		addLayer();
	} else if (layerRemoveBounds.inside(x, y)) {
		removeActiveLayer();
	} else if (layerUpBounds.inside(x, y)) {
		moveActiveLayer(1);
	} else if (layerDownBounds.inside(x, y)) {
		moveActiveLayer(-1);
	} else if (layerAddImageBounds.inside(x, y)) {
		ofFileDialogResult result = ofSystemLoadDialog("Load image to active layer");
		if (result.bSuccess) {
			addImageToActiveLayer(result.getPath());
		}
	} else if (layerRemoveImageBounds.inside(x, y)) {
		removeImageFromActiveLayer();
	} else if (layerOpacityBounds.inside(x, y)) {
		draggingLayerOpacity = true;
		setActiveLayerOpacity(x);
	} else if (layerPanelListBounds.inside(x, y)) {
		for (const auto &row : layerPanelRows) {
			if (!row.bounds.inside(x, y)) {
				continue;
			}
			activeLayerIndex = row.layerIndex;
			selectedLayerItemType = row.type;
			if (row.type == LayerItemType::Layer && layerVisibilityBounds[row.layerIndex].inside(x, y)) {
				layers[row.layerIndex]->visible = !layers[row.layerIndex]->visible;
				rebuildCanvas();
			}
			break;
		}
	}
	return true;
}
