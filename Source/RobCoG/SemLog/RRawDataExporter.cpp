// Fill out your copyright notice in the Description page of Project Settings.

#include "RobCoG.h"
#include "RRawDataExporter.h"


// Set default values
FRRawDataExporter::FRRawDataExporter(
	const float DistThreshSqr, 
	TSharedPtr<IFileHandle> FileHandle,
	TMap<ASkeletalMeshActor*, FString> SkelActPtrToUniqNameMap,
	TMap<AStaticMeshActor*, FString> DynamicActPtrToUniqNameMap,
	TMap<AStaticMeshActor*, FString> StaticActPtrToUniqNameMap) :
	DistanceThresholdSquared(DistThreshSqr),
	RawFileHandle(FileHandle)
{
	// Init items we want to log
	FRRawDataExporter::InitItemsToLog(SkelActPtrToUniqNameMap,
		DynamicActPtrToUniqNameMap,
		StaticActPtrToUniqNameMap);
}

// Destructor
FRRawDataExporter::~FRRawDataExporter()
{
	RawFileHandle.Reset();
}

// Update grasping
void FRRawDataExporter::Update(const float Timestamp)
{
	// Json root object
	TSharedPtr<FJsonObject> JsonRootObj = MakeShareable(new FJsonObject);

	// Set timestamp
	JsonRootObj->SetNumberField("timestamp", Timestamp);

	// Json array of actors
	TArray< TSharedPtr<FJsonValue> > JsonActorArr;

	// Iterate through the skeletal mesh components
	for (auto& SkelActStructItr : SkelActStructArr)
	{
		// Get component current location
		const FVector CurrCompLocation = SkelActStructItr.SkelMeshComp->GetActorLocation();

		// Squared distance between the current and the previous pose
		const float DistSqr = FVector::DistSquared(CurrCompLocation, SkelActStructItr.PrevLoc);

		// Save data if distance larger than threshold
		if (DistSqr > DistanceThresholdSquared)
		{
			// Get a local pointer of the skeletal mesh
			USkeletalMeshComponent* CurrSkelMesh = SkelActStructItr.SkelMeshComp->GetSkeletalMeshComponent();
			// Update previous location
			SkelActStructItr.PrevLoc = CurrCompLocation;
			// Json actor object with name location and rotation
			TSharedPtr<FJsonObject> JsonActorObj = FRRawDataExporter::CreateNameLocRotJsonObject(
				SkelActStructItr.UniqueName, CurrCompLocation * 0.01, CurrSkelMesh->GetComponentQuat());

			// Json array of bones
			TArray< TSharedPtr<FJsonValue> > JsonBoneArr;
			// Get bone names
			TArray<FName> BoneNames;
			CurrSkelMesh->GetBoneNames(BoneNames);

			// Iterate through the bones of the skeletal mesh
			for (const auto BoneName : BoneNames)
			{
				// TODO black voodo magic crashes, bug report, crashes if this is not called before
				CurrSkelMesh->GetBoneQuaternion(BoneName);

				// Json bone object with name location and rotation
				TSharedPtr<FJsonObject> JsonBoneObj = FRRawDataExporter::CreateNameLocRotJsonObject(
					BoneName.ToString(), CurrSkelMesh->GetBoneLocation(BoneName) * 0.01,
					CurrSkelMesh->GetBoneQuaternion(BoneName));

				// Add bone to Json array
				JsonBoneArr.Add(MakeShareable(new FJsonValueObject(JsonBoneObj)));
			}
			// Add bones to Json actor
			JsonActorObj->SetArrayField("bones", JsonBoneArr);

			// Add actor to Json array
			JsonActorArr.Add(MakeShareable(new FJsonValueObject(JsonActorObj)));
		}
	}

	// Iterate through the static mesh components
	for (auto& DynamicActStructItr : DynamicActStructArr)
	{
		// Get component current location
		const FVector CurrActLocation = DynamicActStructItr.StaticMeshAct->GetActorLocation();

		// Squared distance between the current and the previous pose
		const float DistSqr = FVector::DistSquared(CurrActLocation, DynamicActStructItr.PrevLoc);

		// Save data if distance larger than threshold
		if (DistSqr > DistanceThresholdSquared)
		{
			// Get a local pointer of the skeletal mesh actor
			AStaticMeshActor* CurrStaticMeshAct = DynamicActStructItr.StaticMeshAct;
			// Update previous location
			DynamicActStructItr.PrevLoc = CurrActLocation;

			// Json actor object with name location and rotation
			TSharedPtr<FJsonObject> JsonActorObj = FRRawDataExporter::CreateNameLocRotJsonObject(
				DynamicActStructItr.UniqueName, CurrActLocation * 0.01, CurrStaticMeshAct->GetActorQuat());

			// Add actor to Json array
			JsonActorArr.Add(MakeShareable(new FJsonValueObject(JsonActorObj)));
		}
	}

	// Check if static map actors need to be logged
	if (StaticActToUniqName.Num() > 0)
	{
		// Iterate the static map actors (done only once)
		for (auto& StaticActToUniqNameItr : StaticActToUniqName)
		{
			// Json actor object with name location and rotation
			TSharedPtr<FJsonObject> JsonActorObj = FRRawDataExporter::CreateNameLocRotJsonObject(
				StaticActToUniqNameItr.Value, StaticActToUniqNameItr.Key->GetActorLocation() * 0.01, StaticActToUniqNameItr.Key->GetActorQuat());

			// Add actor to Json array
			JsonActorArr.Add(MakeShareable(new FJsonValueObject(JsonActorObj)));
		}
		// Empty array, only needs to be logged once;
		StaticActToUniqName.Empty();
	}

	
	// Add actors to Json root
	JsonRootObj->SetArrayField("actors", JsonActorArr);

	// Transform to string
	FString JsonOutputString;
	TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&JsonOutputString);
	FJsonSerializer::Serialize(JsonRootObj.ToSharedRef(), Writer);

	// Write string to file
	RawFileHandle->Write((const uint8*)TCHAR_TO_ANSI(*JsonOutputString), JsonOutputString.Len());
}

// Create Json object with a 3d location
inline TSharedPtr<FJsonObject> FRRawDataExporter::CreateLocationJsonObject(const FVector Location)
{
	// Json location object
	TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject);
	// Add fields
	JsonObj->SetNumberField("x", Location.X);
	JsonObj->SetNumberField("y", -Location.Y); // left to right handed
	JsonObj->SetNumberField("z", Location.Z);
	
	return JsonObj;
}

// Create Json object with a 3d rotation as quaternion 
inline TSharedPtr<FJsonObject> FRRawDataExporter::CreateRotationJsonObject(const FQuat Rotation)
{
	// Json rotation object
	TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject);
	// Add fields
	JsonObj->SetNumberField("w", Rotation.W);
	JsonObj->SetNumberField("x", -Rotation.X); // left to right handed
	JsonObj->SetNumberField("y", Rotation.Y);
	JsonObj->SetNumberField("z", -Rotation.Z); // left to right handed

	return JsonObj;
}

// Create Json object with name location and rotation
inline TSharedPtr<FJsonObject> FRRawDataExporter::CreateNameLocRotJsonObject(const FString Name, const FVector Location, const FQuat Rotation)
{
	// Json  actor object
	TSharedPtr<FJsonObject> JsonObj = MakeShareable(new FJsonObject);
	// Add fields
	JsonObj->SetStringField("name", Name);
	JsonObj->SetObjectField("pos", FRRawDataExporter::CreateLocationJsonObject(Location));
	JsonObj->SetObjectField("rot", FRRawDataExporter::CreateRotationJsonObject(Rotation));

	return JsonObj;
}

// Update grasping
void FRRawDataExporter::InitItemsToLog(TMap<ASkeletalMeshActor*, FString>& SkelActPtrToUniqNameMap,
	TMap<AStaticMeshActor*, FString>& DynamicActPtrToUniqNameMap,
	TMap<AStaticMeshActor*, FString>& StaticActPtrToUniqNameMap)
{
	UE_LOG(LogTemp, Warning, TEXT("Skeletal components to be logged:"));
	for (const auto SkelActPtrToUniqNameItr : SkelActPtrToUniqNameMap)
	{
		SkelActStructArr.Add(
			FRSkelLogRawStruct(SkelActPtrToUniqNameItr.Key, SkelActPtrToUniqNameItr.Value));
	}

	UE_LOG(LogTemp, Warning, TEXT("Dynamic items to be logged:"));
	for (const auto DynamicActPtrToUniqNameItr : DynamicActPtrToUniqNameMap)
	{
		DynamicActStructArr.Add(
			FRDynActLogRawStruct(DynamicActPtrToUniqNameItr.Key, DynamicActPtrToUniqNameItr.Value));
	}

	StaticActToUniqName = StaticActPtrToUniqNameMap;
}
