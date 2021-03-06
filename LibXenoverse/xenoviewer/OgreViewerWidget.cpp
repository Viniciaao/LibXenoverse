#include "EMDOgre.h"
#include "EMMOgre.h"
#include "EMBOgre.h"
#include "ESKOgre.h"
#include "EANOgre.h"
#include "SkeletonDebug.h"
#include "EMDRenderObjectListener.h"
#include "ViewerGrid.h"
#include "OgreWidget.h"

namespace QtOgre
{
	void OgreWidget::loadDebugModels() {
		// Load Character Models/Skeletons/Animations/Materials/Textures
		string folder = "";
		string character_name = "GOK";
		string character_index = "000";
		string character_prefix = folder + character_name + "/" + character_name + "_" + character_index;
		string skeleton_filename = character_prefix + ".esk";
		string animation_filename = "GOK/GOK.ean";

		ESKOgre *skeleton = NULL;
		skeleton = new ESKOgre();
		if (skeleton->load(skeleton_filename)) {
			skeleton->createOgreSkeleton(mSceneMgr);
		}
		else {
			delete skeleton;
			skeleton = NULL;
		}

		animation = new EANOgre();
		if (animation->load(animation_filename)) {
			animation->createOgreAnimations(skeleton);
		}
		else {
			SHOW_SMSG("Couldn't load " + animation_filename + " for animation.");
			delete animation;
			animation = NULL;
		}

		vector<string> model_names;
		model_names.push_back(character_prefix + "_Bust");
		model_names.push_back(character_prefix + "_Boots");
		model_names.push_back(character_prefix + "_Face_base");
		model_names.push_back(character_prefix + "_Face_eye");
		model_names.push_back(character_prefix + "_Face_forehead");
		model_names.push_back(character_prefix + "_Pants");
		model_names.push_back(character_prefix + "_Rist");

		for (size_t i = 0; i < model_names.size(); i++) {
			string emb_filename = model_names[i] + ".emb";
			string emb_dyt_filename = model_names[i] + ".dyt.emb";

			EMBOgre *texture_pack = new EMBOgre();
			if (texture_pack->load(emb_filename)) {
				texture_pack->createOgreTextures();
			}
			else {
				delete texture_pack;
				texture_pack = NULL;
			}

			EMBOgre *texture_dyt_pack = new EMBOgre();
			if (texture_dyt_pack->load(emb_dyt_filename)) {
				texture_dyt_pack->createOgreTextures();
			}
			else {
				delete texture_dyt_pack;
				texture_dyt_pack = NULL;
			}

			EMMOgre *material = new EMMOgre();
			if (material->load(model_names[i] + ".emm")) {
				material->setTexturePack(texture_pack);
				material->setDYTTexturePack(texture_dyt_pack);
				material->createOgreMaterials();
			}
			else {
				delete material;
				material = NULL;
			}

			EMDOgre *model = new EMDOgre();
			if (model->load(model_names[i] + ".emd")) {
				if (skeleton) {
					model->setSkeleton(skeleton);
				}

				model->setMaterialPack(material);

				Ogre::SceneNode *emd_root_node = model->createOgreSceneNode(mSceneMgr);
			}
			else {
				delete model;
				model = NULL;
			}

			delete model;
			delete texture_pack;
			delete texture_dyt_pack;
			delete material;
		}
	}

	void OgreWidget::createScene(void) {
		LibXenoverse::initializeDebuggingLog();

		// Initialize
		current_animation_index = 0;
		skeleton_debug = NULL;
		current_animation_state = NULL;
		animation = NULL;
		entity = NULL;
		enable_spinning = false;
		enable_panning = false;
		spin_x = 0.0f;
		spin_y = 0.0f;
		resetCamera();

		// Create a grid
		ViewerGrid *viewer_grid = new ViewerGrid();
		viewer_grid->createSceneNode(mSceneMgr);
		
		// Create a blank texture
		Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().createManual("Blank", XENOVIEWER_RESOURCE_GROUP, Ogre::TEX_TYPE_2D, 32, 32, 0, Ogre::PF_BYTE_BGRA, Ogre::TU_DEFAULT);
		Ogre::HardwarePixelBufferSharedPtr pixelBuffer = texture->getBuffer();
		pixelBuffer->lock(Ogre::HardwareBuffer::HBL_NORMAL);
		const Ogre::PixelBox& pixelBox = pixelBuffer->getCurrentLock();
		Ogre::uint8* pDest = static_cast<Ogre::uint8*>(pixelBox.data);

		for (size_t j = 0; j < 32; j++) {
			for (size_t i = 0; i < 32; i++) {
				*pDest++ = 0;
				*pDest++ = 0;
				*pDest++ = 0;
				*pDest++ = 255;
			}

			pDest += pixelBox.getRowSkip() * Ogre::PixelUtil::getNumElemBytes(pixelBox.format);
		}
		pixelBuffer->unlock();

		// Create listener for EMDs
		emd_render_object_listener = new EMDRenderObjectListener();
		mSceneMgr->addRenderObjectListener(emd_render_object_listener);

		// Create Lighting
		mSceneMgr->setAmbientLight(Ogre::ColourValue(0.5, 0.5, 0.5));
		Ogre::Light *direct_light = mSceneMgr->createLight("Xenoviewer Direct Light");
		direct_light->setSpecularColour(Ogre::ColourValue::White);
		direct_light->setDiffuseColour(Ogre::ColourValue(1.0, 1.0, 1.0));
		direct_light->setType(Ogre::Light::LT_DIRECTIONAL);
		direct_light->setDirection(Ogre::Vector3(1, -1, 1).normalisedCopy());

		// Load Shaders
		vector<string> shader_names;
		shader_names.push_back("adam_shader/shader_age_ps.emb");
		shader_names.push_back("adam_shader/shader_age_vs.emb");
		shader_names.push_back("adam_shader/shader_default_ps.emb");
		shader_names.push_back("adam_shader/shader_default_vs.emb");

		bool needs_install_shaders = false;
		for (size_t i = 0; i < shader_names.size(); i++) {
			if (!LibXenoverse::fileCheck(shader_names[i])) {
				needs_install_shaders = true;
				break;
			}
		}

		if (needs_install_shaders) {
			if (!installShaders()) {
				return;
			}
		}

		for (size_t i = 0; i < shader_names.size(); i++) {
			EMBOgre *shader_pack = new EMBOgre();
			if (shader_pack->load(shader_names[i])) {
				shader_pack->createOgreShaders();
			}
			else {
				SHOW_ERROR(QString("Couldn't load Shader Pack %1. File is either missing, open by another application, or corrupt.").arg(shader_names[i].c_str()));
			}
		}

		//loadDebugModels();
	}

	bool OgreWidget::frameRenderingQueued(const Ogre::FrameEvent& evt) {
		for (list<EANOgre *>::iterator it = ean_list.begin(); it != ean_list.end(); it++) {
			EANAnimation *force_animation = (*it)->toForceAnimation();

			if (force_animation) {
				for (list<ESKOgre *>::iterator it = esk_list.begin(); it != esk_list.end(); it++) {
					(*it)->tagAnimationChange(force_animation);
				}
			}
		}

		for (list<ESKOgre *>::iterator it = esk_list.begin(); it != esk_list.end();) {
			if ((*it)->toDelete()) {
				for (list<EMDOgre *>::iterator itm = emd_list.begin(); itm != emd_list.end(); itm++) {
					if ((*itm)->getSkeleton() == *it) {
						(*itm)->setSkeleton(NULL);
						(*itm)->tagForRebuild();
					}
				}
				delete *it;
				it = esk_list.erase(it);
				continue;
			}

			if ((*it)->changedAnimation()) {
				(*it)->changeAnimation();
			}

			Ogre::AnimationState *state = (*it)->getCurrentAnimationState();
			if (state) {
				state->addTime(evt.timeSinceLastFrame);
			}

			it++;
		}

		for (list<EMDOgre *>::iterator it = emd_list.begin(); it != emd_list.end();) {
			if ((*it)->toDelete()) {
				delete *it;
				it = emd_list.erase(it);
				continue;
			}

			if ((*it)->toRebuild()) {
				(*it)->rebuild();
			}

			it++;
		}

		repositionCamera();
		return true;
	}

	void OgreWidget::mousePressEvent(QMouseEvent * event) {
		if (event->button() == Qt::LeftButton) {
			enable_spinning = true;
		}

		if (event->button() == Qt::RightButton) {
			enable_panning = true;
		}
	}

	void OgreWidget::mouseMoveEvent(QMouseEvent * event) {
		int mouse_x = event->x();
		int mouse_y = event->y();
		int delta_x = mouse_x - last_mouse_x;
		int delta_y = mouse_y - last_mouse_y;
		float delta_f_x = (float)delta_x / (float)width();
		float delta_f_y = (float)delta_y / (float)height();

		if (enable_spinning) {
			spinCamera(delta_f_x, delta_f_y);
		}

		if (enable_panning) {
			panCamera(delta_f_x, delta_f_y);
		}

		last_mouse_x = mouse_x;
		last_mouse_y = mouse_y;
	}

	void OgreWidget::mouseReleaseEvent(QMouseEvent * event) {
		if (event->button() == Qt::LeftButton) {
			enable_spinning = false;
		}

		if (event->button() == Qt::RightButton) {
			enable_panning = false;
		}
	}

	void OgreWidget::wheelEvent(QWheelEvent * event) {
		zoomCamera(event->delta());
	}

	void OgreWidget::spinCamera(float delta_x, float delta_y) {
		viewer_angle_x += delta_x * -4.0f;
		viewer_angle_y += delta_y * -4.0f;

		if (viewer_angle_x >= Ogre::Math::TWO_PI) viewer_angle_x -= Ogre::Math::TWO_PI;
		if (viewer_angle_x < 0) viewer_angle_x += Ogre::Math::TWO_PI;

		if (viewer_angle_y >= Ogre::Math::HALF_PI - 0.1) viewer_angle_y = Ogre::Math::HALF_PI - 0.1;
		if (viewer_angle_y < -Ogre::Math::HALF_PI + 0.1) viewer_angle_y = -Ogre::Math::HALF_PI + 0.1;
	}

	void OgreWidget::panCamera(float delta_x, float delta_y) {
		viewer_center += mCamera->getOrientation() * Ogre::Vector3(delta_x * -2.0f, -delta_y * -2.0f, 0.0);
	}

	void OgreWidget::zoomCamera(float delta) {
		zoom += delta * -0.001f;

		if (zoom < 0.01f) zoom = 0.01f;
	}

	void OgreWidget::repositionCamera() {
		Ogre::Quaternion rotation_x;
		Ogre::Quaternion rotation_y;
		rotation_x.FromAngleAxis(Ogre::Radian(viewer_angle_x), Ogre::Vector3::UNIT_Y);
		rotation_y.FromAngleAxis(Ogre::Radian(viewer_angle_y), Ogre::Vector3::UNIT_X);

		Ogre::Vector3 new_position = viewer_center + ((rotation_x * rotation_y) * Ogre::Vector3(0, 0, 3.0 * zoom));
		mCamera->setPosition(new_position);
		mCamera->lookAt(viewer_center);
	}

	void OgreWidget::resetCamera() {
		viewer_center = Ogre::Vector3(0, 0.5, 0);
		viewer_angle_x = Ogre::Math::PI;
		viewer_angle_y = 0.0f;
		zoom = 1.0f;
	}

	void OgreWidget::addFileEAN(string filename, list<EANOgre *> &target_ean_list) {
		string ean_name = LibXenoverse::nameFromFilenameNoExtension(filename, true);

		// Search for an EAN with the same name
		for (list<EANOgre *>::iterator it = ean_list.begin(); it != ean_list.end(); it++) {
			if ((*it)->getName() == ean_name) {
				SHOW_ERROR("A EAN Animation Pack with the name " + QString(ean_name.c_str()) + " already exists! (FIXME: Implement prompt for replacing already loaded files)");
				return;
			}
		}

		EANOgre *animation = new EANOgre();
		if (animation->load(filename)) {
			for (list<ESKOgre *>::iterator it = esk_list.begin(); it != esk_list.end(); it++) {
				animation->createOgreAnimations(*it);
				(*it)->refreshAnimations();
			}

			ean_list.push_back(animation);
			target_ean_list.push_back(animation);
		}
		else {
			delete animation;
			SHOW_ERROR("Invalid EAN Animation Pack. Is " + QString(filename.c_str()) + " valid?");
			return;
		}
	}

	void OgreWidget::addFileESK(string filename, list<ESKOgre *> &target_esk_list) {
		string esk_name = LibXenoverse::nameFromFilenameNoExtension(filename, true);

		// Search for an ESK with the same name
		for (list<ESKOgre *>::iterator it = esk_list.begin(); it != esk_list.end(); it++) {
			if ((*it)->getName() == esk_name) {
				SHOW_ERROR("A ESK Skeleton with the name " + QString(esk_name.c_str()) + " already exists! (FIXME: Implement prompt for replacing already loaded files)");
				return;
			}
		}

		ESKOgre *skeleton = new ESKOgre();
		if (skeleton->load(filename)) {
			skeleton->createOgreSkeleton(mSceneMgr);
			esk_list.push_back(skeleton);
			target_esk_list.push_back(skeleton);

			for (list<EANOgre *>::iterator it = ean_list.begin(); it != ean_list.end(); it++) {
				(*it)->createOgreAnimations(skeleton);
			}

			skeleton->refreshAnimations();
		}
		else {
			delete skeleton;
			SHOW_ERROR("Invalid ESK Skeleton. Is " + QString(filename.c_str()) + " valid?");
			return;
		}
	}

	void OgreWidget::addFileEMD(string filename, list<EMDOgre *> &target_emd_list) {
		string emb_filename     = LibXenoverse::filenameNoExtension(filename) + ".emb";
		string emb_dyt_filename = LibXenoverse::filenameNoExtension(filename) + ".dyt.emb";
		string emm_filename     = LibXenoverse::filenameNoExtension(filename) + ".emm";
		string emd_name         = LibXenoverse::nameFromFilenameNoExtension(filename, true);

		// Search for an EMD with the same name
		for (list<EMDOgre *>::iterator it = emd_list.begin(); it != emd_list.end(); it++) {
			if ((*it)->getName() == emd_name) {
				SHOW_ERROR("A EMD Model Pack with the name " + QString(emd_name.c_str()) + " already exists! (FIXME: Implement prompt for replacing already loaded files)");
				return;
			}
		}

		EMBOgre *texture_pack = NULL;
		EMBOgre *texture_dyt_pack = NULL;
		EMMOgre *material = NULL;
		EMDOgre *model = NULL;

		if (!LibXenoverse::fileCheck(emb_filename)) {
			SHOW_ERROR("No EMB Pack with the name " + QString(emb_filename.c_str()) + " found. Make sure it's on the same folder as the EMD file you're adding and it's not open by any other application!");
			return;
		}

		if (!LibXenoverse::fileCheck(emb_dyt_filename)) {
			SHOW_ERROR("No EMB DYT Pack with the name " + QString(emb_dyt_filename.c_str()) + " found. Make sure it's on the same folder as the EMD file you're adding and it's not open by any other application!");
			return;
		}

		if (!LibXenoverse::fileCheck(emm_filename)) {
			SHOW_ERROR("No EMM Pack with the name " + QString(emm_filename.c_str()) + " found. Make sure it's on the same folder as the EMD file you're adding and it's not open by any other application!");
			return;
		}

		texture_pack = new EMBOgre();
		if (texture_pack->load(emb_filename)) {
			texture_pack->createOgreTextures();
		}
		else {
			SHOW_ERROR("Invalid EMB Texture Pack. Is " + QString(emb_filename.c_str()) + " valid?");
			goto abort_clean;
		}

		texture_dyt_pack = new EMBOgre();
		if (texture_dyt_pack->load(emb_dyt_filename)) {
			texture_dyt_pack->createOgreTextures();
		}
		else {
			SHOW_ERROR("Invalid EMB DYT Texture Pack. Is " + QString(emb_dyt_filename.c_str()) + " valid?");
			goto abort_clean;
		}

		material = new EMMOgre();
		if (material->load(emm_filename)) {
			material->setTexturePack(texture_pack);
			material->setDYTTexturePack(texture_dyt_pack);
			material->createOgreMaterials();
		}
		else {
			SHOW_ERROR("Invalid EMM Material Pack. Is " + QString(emm_filename.c_str()) + " valid?");
			goto abort_clean;
		}

		model = new EMDOgre();
		if (model->load(filename)) {
			model->setMaterialPack(material);
			Ogre::SceneNode *emd_root_node = model->createOgreSceneNode(mSceneMgr);
			emd_list.push_back(model);
			target_emd_list.push_back(model);
		}
		else {
			SHOW_ERROR("Invalid EMD Model Pack. Is " + QString(filename.c_str()) + " valid?");
			goto abort_clean;
		}
		return;

	abort_clean:
		delete material;
		delete texture_pack;
		delete texture_dyt_pack;
		delete model;
		return;
	}

	void OgreWidget::addFile(string filename, list<EMDOgre *> &target_emd_list, list<ESKOgre *> &target_esk_list, list<EANOgre *> &target_ean_list) {
		string extension = LibXenoverse::extensionFromFilename(filename, true);

		if (extension == "emd") {
			addFileEMD(filename, target_emd_list);
		}
		else if (extension == "esk") {
			addFileESK(filename, target_esk_list);
		}
		else if (extension == "ean") {
			addFileEAN(filename, target_ean_list);
		}
		else if (extension == "emb") {
			SHOW_ERROR("EMB files are automatically loaded with the EMD file. Load the EMD file instead!");
		}
		else if (extension == "emm") {
			SHOW_ERROR("EMM files are automatically loaded with the EMD file. Load the EMD file instead!");
		}
		else {
			SHOW_ERROR("File Extension " + QString(extension.c_str()) + " for file " + QString(filename.c_str()) + " not recognized. Xenoviewer can't read this format.");
		}
	}

	void OgreWidget::addFiles(const QStringList& pathList, list<EMDOgre *> &target_emd_list, list<ESKOgre *> &target_esk_list, list<EANOgre *> &target_ean_list) {
		for (QStringList::const_iterator it = pathList.begin(); it != pathList.end(); it++) {
			string filename = (*it).toStdString();
			addFile(filename, target_emd_list, target_esk_list, target_ean_list);
		}
	}

	void OgreWidget::getItemLists(list<EMDOgre *> &target_emd_list, list<ESKOgre *> &target_esk_list, list<EANOgre *> &target_ean_list) {
		target_emd_list = emd_list;
		target_esk_list = esk_list;
		target_ean_list = ean_list;
	}

	bool OgreWidget::installShaders() {
		QFileDialog dialog(this);
		dialog.setFileMode(QFileDialog::Directory);

		QMessageBox::about(NULL, "Xenoviewer Installation", "For previewing materials correctly, Xenoviewer requires "
								 "installing the game's shaders for the first time. Select the directory <b>adam_shader</b> "
								 "inside the extracted contents of <b>data.cpk</b> after pressing 'Ok'.");

		QString dir = QFileDialog::getExistingDirectory(this, "Choose Shader Folder");

		if (dir.size()) {
			// Make Shader Folder
			QDir().mkdir("adam_shader");

			string dir_std = dir.toStdString();

			vector<string> shader_names;
			shader_names.push_back("shader_age_ps");
			shader_names.push_back("shader_age_vs");
			shader_names.push_back("shader_default_ps");
			shader_names.push_back("shader_default_vs");

			for (size_t i = 0; i < shader_names.size(); i++) {
				string full_path = dir_std + "/" + shader_names[i] + ".emz";

				EMZ *emz_pack = new EMZ();
				if (emz_pack->load(full_path)) {
					string new_extension = emz_pack->detectNewExtension();
					string new_filename = "adam_shader/" + shader_names[i] + new_extension;
					emz_pack->saveUncompressed(new_filename);
				}
				delete emz_pack;
			}
		}
		else {
			return false;
		}

		return true;
	}
}
