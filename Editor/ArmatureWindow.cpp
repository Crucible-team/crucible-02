#include "stdafx.h"
#include "ArmatureWindow.h"

using namespace wi::ecs;
using namespace wi::scene;

void ArmatureWindow::Create(EditorComponent* _editor)
{
	editor = _editor;

	wi::gui::Window::Create(ICON_ARMATURE " Armature", wi::gui::Window::WindowControls::COLLAPSE | wi::gui::Window::WindowControls::CLOSE | wi::gui::Window::WindowControls::FIT_ALL_WIDGETS_VERTICAL);
	SetSize(XMFLOAT2(670, 380));

	closeButton.SetTooltip("Delete ArmatureComponent");
	OnClose([=](wi::gui::EventArgs args) {

		wi::Archive& archive = editor->AdvanceHistory();
		archive << EditorComponent::HISTORYOP_COMPONENT_DATA;
		editor->RecordEntity(archive, entity);

		editor->GetCurrentScene().armatures.Remove(entity);

		editor->RecordEntity(archive, entity);

		editor->componentsWnd.RefreshEntityTree();
		});

	float x = 60;
	float y = 4;
	float hei = 20;
	float step = hei + 2;
	float wid = 220;

	infoLabel.Create("");
	infoLabel.SetText("This window will stay open even if you select other entities until it is collapsed, so you can select other bone entities.");
	infoLabel.SetFitTextEnabled(true);
	AddWidget(&infoLabel);

	resetPoseButton.Create("Reset Pose");
	resetPoseButton.SetTooltip("Reset Pose will be performed on the Armature, based on the bone inverse bind matrices.");
	resetPoseButton.SetSize(XMFLOAT2(wid, hei));
	resetPoseButton.OnClick([=](wi::gui::EventArgs args) {
		wi::scene::Scene& scene = editor->GetCurrentScene();
		scene.ResetPose(entity);
	});
	AddWidget(&resetPoseButton);

	createHumanoidButton.Create("Try to create humanoid rig");
	createHumanoidButton.SetTooltip("Tries to create a humanoid component based on bone naming convention.\nIt supports the VRM and Mixamo naming convention.");
	createHumanoidButton.SetSize(XMFLOAT2(wid, hei));
	createHumanoidButton.OnClick([=](wi::gui::EventArgs args) {
		wi::scene::Scene& scene = editor->GetCurrentScene();
		if (scene.humanoids.Contains(entity))
		{
			wi::helper::messageBox("Humanoid Component already exists!");
			return;
		}

		const ArmatureComponent* armature = scene.armatures.GetComponent(entity);
		if (armature == nullptr)
			return;

		static const wi::unordered_map<HumanoidComponent::HumanoidBone, wi::vector<std::string>> mapping = {
			{HumanoidComponent::HumanoidBone::Hips, {"Hips", "pelvis", "Bip02 Pelvis","Bip01 Pelvis","bip01 pelvis","ValveBiped.Bip01_Pelvis"}},
			{HumanoidComponent::HumanoidBone::Spine, {"Spine", "spine_01","Bip02 Spine","Bip01 Spine","bip01 spine","ValveBiped.Bip01_Spine"}},
			{HumanoidComponent::HumanoidBone::Chest, {"Chest", "Spine1", "spine_02","Bip02 Spine1","Bip01 Spine2","bip01 spine1","ValveBiped.Bip01_Spine1"}},
			{HumanoidComponent::HumanoidBone::UpperChest, {"UpperChest", "Spine2", "spine_03","Bip02 Spine2","Bip01 Spine3","Bip01 spine3","ValveBiped.Bip01_Spine4"}},
			{HumanoidComponent::HumanoidBone::Spine3, {"ValveBiped.Bip01_Spine2"}},
			{HumanoidComponent::HumanoidBone::Neck, {"Neck","Bip02 Neck","Bip01 Neck","Bip01 neck","ValveBiped.Bip01_Neck1"}},

			{HumanoidComponent::HumanoidBone::Head, {"Head","Bip02 Head","Bip01 Head","bip01 head","ValveBiped.Bip01_Head1"}},
			{HumanoidComponent::HumanoidBone::LeftEye, {"LeftEye","Bip01 L Eye","bip01 l eye","ValveBiped.Bip01_L_Eye"}},
			{HumanoidComponent::HumanoidBone::RightEye, {"RightEye","Bip01 R Eye","bip01 r eye","ValveBiped.Bip01_R_Eye"}},
			{HumanoidComponent::HumanoidBone::Jaw, {"Jaw","Bone01","Mouth"}},
			{HumanoidComponent::HumanoidBone::Forward, {"ValveBiped.forward","forward","Forward"}},
			{HumanoidComponent::HumanoidBone::LeftTrapezius, {"ValveBiped.Bip01_L_Trapezius","L_Trapezius", "L Trapezius","l trapezius"}},
			{HumanoidComponent::HumanoidBone::RightTrapezius, {"ValveBiped.Bip01_R_Trapezius","R_Trapezius","R Trapezius","r trapezius"}},
			{HumanoidComponent::HumanoidBone::LeftBicep, {"ValveBiped.Bip01_L_Bicep","L_Bicep","L Bicep","l bicep"}},
			{HumanoidComponent::HumanoidBone::RightBicep, {"ValveBiped.Bip01_R_Bicep","R_Bicep","R Bicep","r bicep"}},
			{HumanoidComponent::HumanoidBone::LeftUlna, {"ValveBiped.Bip01_L_Ulna","L_Ulna","L Ulna","l ulna"}},
			{HumanoidComponent::HumanoidBone::RightUlna, {"ValveBiped.Bip01_R_Ulna","R_Ulna","R Ulna","r ulna"}},
			{HumanoidComponent::HumanoidBone::LeftWrist, {"ValveBiped.Bip01_L_Wrist","L_Wrist","L Wrist","l wrist"}},
			{HumanoidComponent::HumanoidBone::RightWrist, {"ValveBiped.Bip01_R_Wrist","R_Wrist","R Wrist","r wrist"}},

			{HumanoidComponent::HumanoidBone::LeftUpperLeg, {"LeftUpperLeg", "LeftUpLeg", "thigh_l","Bip02 L Leg","Bip01 L Thigh","bip01 l thigh","ValveBiped.Bip01_L_Thigh"}},
			{HumanoidComponent::HumanoidBone::LeftLowerLeg, {"LeftLowerLeg", "LeftLeg", "calf_l","Bip02 L Leg1","Bip01 L Calf","bip01 l calf","ValveBiped.Bip01_L_Calf"}},
			{HumanoidComponent::HumanoidBone::LeftFoot, {"LeftFoot", "foot_l","Bip02 L Foot","Bip01 L Foot","bip01 l foot","ValveBiped.Bip01_L_Foot"}},
			{HumanoidComponent::HumanoidBone::LeftToes, {"LeftToe", "ball_l","Bip01 L Toe0","bip01 l toe","ValveBiped.Bip01_L_Toe0"}},
			{HumanoidComponent::HumanoidBone::RightUpperLeg, {"RightUpperLeg", "RightUpLeg", "thigh_r","Bip02 R Leg","Bip01 R Thigh","bip01 r thigh","ValveBiped.Bip01_R_Thigh"}},
			{HumanoidComponent::HumanoidBone::RightLowerLeg, {"RightLowerLeg", "RightLeg", "calf_r","Bip02 R Leg1","Bip01 R Calf","bip01 r calf","ValveBiped.Bip01_R_Calf"}},
			{HumanoidComponent::HumanoidBone::RightFoot, {"RightFoot", "foot_r","Bip02 R Foot","Bip01 R Foot","bip01 r foot","ValveBiped.Bip01_R_Foot"}},
			{HumanoidComponent::HumanoidBone::RightToes, {"RightToe", "ball_r","Bip01 R Toe0","bip01 r toe","ValveBiped.Bip01_R_Toe0"}},

			{HumanoidComponent::HumanoidBone::LeftShoulder, {"LeftShoulder", "clavicle_l","Bip02 L Arm","Bip01 L Clavicle","bip01 l clavicle","ValveBiped.Bip01_L_Clavicle"}},
			{HumanoidComponent::HumanoidBone::LeftUpperArm, {"LeftUpperArm", "LeftArm", "upperarm_l","Bip02 L Arm1","Bip01 L UpperArm","Bip01 L UpperArm","bip01 l upperarm","ValveBiped.Bip01_L_UpperArm"}},
			{HumanoidComponent::HumanoidBone::LeftLowerArm, {"LeftLowerArm", "LeftForeArm", "lowerarm_l","Bip02 L Arm2","Bip01 L Forearm","bip01 l forearm","ValveBiped.Bip01_L_Elbow"}},
			{HumanoidComponent::HumanoidBone::LeftHand, {"LeftHand", "hand_l","Bip02 L Hand","Bip01 L Hand","bip01 l hand","ValveBiped.Bip01_L_Hand"}},
			{HumanoidComponent::HumanoidBone::RightShoulder, {"RightShoulder", "clavicle_r","Bip02 R Arm","Bip01 R Clavicle","bip01 r clavicle","ValveBiped.Bip01_R_Clavicle"}},
			{HumanoidComponent::HumanoidBone::RightUpperArm, {"RightUpperArm", "RightArm", "upperarm_r","Bip02 R Arm1","Bip01 R UpperArm","bip01 r upperarm","ValveBiped.Bip01_R_UpperArm"}},
			{HumanoidComponent::HumanoidBone::RightLowerArm, {"RightLowerArm", "RightForeArm", "lowerarm_r","Bip02 R Arm2","Bip01 R Forearm","bip01 r forearm","ValveBiped.Bip01_R_Elbow"}},
			{HumanoidComponent::HumanoidBone::RightHand, {"RightHand", "hand_r","Bip02 R Hand","Bip01 R Hand","bip01 r hand","ValveBiped.Bip01_R_Hand"}},

			{HumanoidComponent::HumanoidBone::LeftThumbMetacarpal, {"LeftThumbMetacarpal", "LeftHandThumb1", "thumb_01_l","Bip01 L Finger0","ValveBiped.Bip01_L_Finger0"}},
			{HumanoidComponent::HumanoidBone::LeftThumbProximal, {"LeftThumbProximal", "LeftHandThumb2", "thumb_02_l","Bip01 L Finger01","ValveBiped.Bip01_L_Finger01"}},
			{HumanoidComponent::HumanoidBone::LeftThumbDistal, {"LeftThumbDistal", "LeftHandThumb3", "thumb_03_l","Bip01 L Finger02","ValveBiped.Bip01_L_Finger02"}},
			{HumanoidComponent::HumanoidBone::LeftIndexProximal, {"LeftIndexProximal", "LeftHandIndex1", "index_01_l","Bip01 L Finger2","ValveBiped.Bip01_L_Finger1"}},
			{HumanoidComponent::HumanoidBone::LeftIndexIntermediate, {"LeftIndexIntermediate", "LeftHandIndex2", "index_02_l","Bip01 L Finger21","ValveBiped.Bip01_L_Finger11"}},
			{HumanoidComponent::HumanoidBone::LeftIndexDistal, {"LeftIndexDistal", "LeftHandIndex3", "index_03_l","Bip01 L Finger22","ValveBiped.Bip01_L_Finger12"}},
			{HumanoidComponent::HumanoidBone::LeftMiddleProximal, {"LeftMiddleProximal", "LeftHandMiddle1", "middle_01_l","ValveBiped.Bip01_L_Finger2"}},
			{HumanoidComponent::HumanoidBone::LeftMiddleIntermediate, {"LeftMiddleIntermediate", "LeftHandMiddle2", "middle_02_l","ValveBiped.Bip01_L_Finger21"}},
			{HumanoidComponent::HumanoidBone::LeftMiddleDistal, {"LeftMiddleDistal", "LeftHandMiddle3", "middle_03_l","ValveBiped.Bip01_L_Finger22"}},
			{HumanoidComponent::HumanoidBone::LeftRingProximal, {"LeftRingProximal", "LeftHandRing1", "ring_01_l","ValveBiped.Bip01_L_Finger3"}},
			{HumanoidComponent::HumanoidBone::LeftRingIntermediate, {"LeftRingIntermediate", "LeftHandRing2", "ring_02_l","ValveBiped.Bip01_L_Finger31"}},
			{HumanoidComponent::HumanoidBone::LeftRingDistal, {"LeftRingDistal", "LeftHandRing3", "ring_03_l","ValveBiped.Bip01_L_Finger32"}},
			{HumanoidComponent::HumanoidBone::LeftLittleProximal, {"LeftLittleProximal", "LeftHandPinky1", "pinky_01_l","ValveBiped.Bip01_L_Finger4"}},
			{HumanoidComponent::HumanoidBone::LeftLittleIntermediate, {"LeftLittleIntermediate", "LeftHandPinky2", "pinky_02_l","ValveBiped.Bip01_L_Finger41"}},
			{HumanoidComponent::HumanoidBone::LeftLittleDistal, {"LeftLittleDistal", "LeftHandPinky3", "pinky_03_l","ValveBiped.Bip01_L_Finger42"}},
			{HumanoidComponent::HumanoidBone::RightThumbMetacarpal, {"RightThumbMetacarpal", "RightHandThumb1", "thumb_01_r","Bip01 R Finger0","ValveBiped.Bip01_R_Finger0"}},
			{HumanoidComponent::HumanoidBone::RightThumbProximal, {"RightThumbProximal", "RightHandThumb2", "thumb_02_r","Bip01 R Finger01","ValveBiped.Bip01_R_Finger01"}},
			{HumanoidComponent::HumanoidBone::RightThumbDistal, {"RightThumbDistal", "RightHandThumb3", "thumb_03_r","Bip01 R Finger02","ValveBiped.Bip01_R_Finger02"}},
			{HumanoidComponent::HumanoidBone::RightIndexIntermediate, {"RightIndexIntermediate", "RightHandIndex2", "index_01_r","Bip01 R Finger21","ValveBiped.Bip01_R_Finger11"}},
			{HumanoidComponent::HumanoidBone::RightIndexDistal, {"RightIndexDistal", "RightHandIndex3", "index_02_r","Bip01 R Finger22","ValveBiped.Bip01_R_Finger12"}},
			{HumanoidComponent::HumanoidBone::RightIndexProximal, {"RightIndexProximal", "RightHandIndex1", "index_03_r","Bip01 R Finger2","ValveBiped.Bip01_R_Finger1"}},
			{HumanoidComponent::HumanoidBone::RightMiddleProximal, {"RightMiddleProximal", "RightHandMiddle1", "middle_01_r","ValveBiped.Bip01_R_Finger2"}},
			{HumanoidComponent::HumanoidBone::RightMiddleIntermediate, {"RightMiddleIntermediate", "RightHandMiddle2", "middle_02_r","ValveBiped.Bip01_R_Finger21"}},
			{HumanoidComponent::HumanoidBone::RightMiddleDistal, {"RightMiddleDistal", "RightHandMiddle3", "middle_03_r","ValveBiped.Bip01_R_Finger22"}},
			{HumanoidComponent::HumanoidBone::RightRingProximal, {"RightRingProximal", "RightHandRing1", "ring_01_r","ValveBiped.Bip01_R_Finger3"}},
			{HumanoidComponent::HumanoidBone::RightRingIntermediate, {"RightRingIntermediate", "RightHandRing2", "ring_02_r","ValveBiped.Bip01_R_Finger31"}},
			{HumanoidComponent::HumanoidBone::RightRingDistal, {"RightRingDistal", "RightHandRing3", "ring_03_r","ValveBiped.Bip01_R_Finger32"}},
			{HumanoidComponent::HumanoidBone::RightLittleProximal, {"RightLittleProximal", "RightHandPinky1", "pinky_01_r","ValveBiped.Bip01_R_Finger4"}},
			{HumanoidComponent::HumanoidBone::RightLittleIntermediate, {"RightLittleIntermediate", "RightHandPinky2", "pinky_02_r","ValveBiped.Bip01_R_Finger41"}},
			{HumanoidComponent::HumanoidBone::RightLittleDistal, {"RightLittleDistal", "RightHandPinky3", "pinky_03_r","ValveBiped.Bip01_R_Finger42"}},
		};

		HumanoidComponent humanoid;
		bool found_anything = false;

		for (size_t i = 0; i < armature->boneCollection.size(); ++i)
		{
			Entity bone = armature->boneCollection[i];
			NameComponent* name = scene.names.GetComponent(bone);
			if (name == nullptr)
				continue;

			size_t iType = 0;
			for (auto& humanoidBone : humanoid.bones)
			{
				if (humanoidBone == INVALID_ENTITY)
				{
					HumanoidComponent::HumanoidBone type = (HumanoidComponent::HumanoidBone)iType;
					auto it = mapping.find(type);
					if (it != mapping.end())
					{
						for (auto& candidate : it->second)
						{
							if (wi::helper::toUpper(name->name).find(wi::helper::toUpper(candidate)) != std::string::npos)
							{
								humanoidBone = bone;
								found_anything = true;
								break;
							}
						}
					}
				}
				iType++;
			}
		}

		if (found_anything)
		{
			scene.humanoids.Create(entity) = humanoid;
			editor->componentsWnd.humanoidWnd.SetEntity(INVALID_ENTITY);
			editor->componentsWnd.humanoidWnd.SetEntity(entity);
		}
		else
		{
			wi::helper::messageBox("No matching humanoid bones found!");
		}
	});
	AddWidget(&createHumanoidButton);

	boneList.Create("Bones: ");
	boneList.SetSize(XMFLOAT2(wid, 200));
	boneList.SetPos(XMFLOAT2(4, y += step));
	boneList.OnSelect([=](wi::gui::EventArgs args) {

		if (args.iValue < 0)
			return;

		wi::Archive& archive = editor->AdvanceHistory();
		archive << EditorComponent::HISTORYOP_SELECTION;
		// record PREVIOUS selection state...
		editor->RecordSelection(archive);

		editor->translator.selected.clear();

		for (int i = 0; i < boneList.GetItemCount(); ++i)
		{
			const wi::gui::TreeList::Item& item = boneList.GetItem(i);
			if (item.selected)
			{
				wi::scene::PickResult pick;
				pick.entity = (Entity)item.userdata;
				editor->AddSelected(pick);
			}
		}

		// record NEW selection state...
		editor->RecordSelection(archive);

		editor->componentsWnd.RefreshEntityTree();

		});
	AddWidget(&boneList);


	SetMinimized(true);
	SetVisible(false);

	SetEntity(INVALID_ENTITY);
}

void ArmatureWindow::SetEntity(Entity entity)
{
	if (this->entity == entity)
		return;

	Scene& scene = editor->GetCurrentScene();

	const ArmatureComponent* armature = scene.armatures.GetComponent(entity);

	if (armature != nullptr || IsCollapsed())
	{
		this->entity = entity;
		RefreshBoneList();
	}
}
void ArmatureWindow::RefreshBoneList()
{
	Scene& scene = editor->GetCurrentScene();

	const ArmatureComponent* armature = scene.armatures.GetComponent(entity);

	if (armature != nullptr)
	{
		boneList.ClearItems();
		for (Entity bone : armature->boneCollection)
		{
			wi::gui::TreeList::Item item;
			item.userdata = bone;
			item.name += ICON_BONE " ";

			const NameComponent* name = scene.names.GetComponent(bone);
			if (name == nullptr)
			{
				item.name += "[no_name] " + std::to_string(bone);
			}
			else if (name->name.empty())
			{
				item.name += "[name_empty] " + std::to_string(bone);
			}
			else
			{
				item.name += name->name;
			}
			boneList.AddItem(item);
		}
	}
}

void ArmatureWindow::ResizeLayout()
{
	wi::gui::Window::ResizeLayout();
	layout.margin_left = 110;

	layout.add_fullwidth(infoLabel);
	layout.add_fullwidth(resetPoseButton);
	layout.add_fullwidth(createHumanoidButton);

	layout.jump();

	layout.add_fullwidth(boneList);

}
