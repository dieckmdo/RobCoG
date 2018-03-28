// Fill out your copyright notice in the Description page of Project Settings.

#include "RSpawnBox.h"
#include "RobCoG.h"
#include "Kismet/KismetMathLibrary.h"
#include "EngineUtils.h"
#include "Runtime/Engine/Classes/Engine/StaticMeshActor.h"
#include "TimerManager.h"
#include <string>

	// Log for this Class
	DEFINE_LOG_CATEGORY(SB_Log);

ASpawnBox::ASpawnBox()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Sets up the components and initializes the SpawnVolume BoxExtent.
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
	SpawnVolume = CreateDefaultSubobject<UBoxComponent>("SpawnBox");
	SpawnVolume->SetupAttachment(RootComponent);
	SpawnVolume->SetBoxExtent(FVector(40,40,10), true);
	SpawnVolume->bGenerateOverlapEvents = true;

	CameraRadius = 50;
  UpdateTime = 5.0f;
  Angle = 90.f;
  AngleAxis = 0.0f;
	bIsScanning = false;
  bIsWaitingForTimer = false;

}

void ASpawnBox::BeginPlay()
{
	Super::BeginPlay();

	// always start scanning at the height of the SpawnVolume
	CameraHeight = SpawnVolume->GetComponentLocation().Z;

  if(!ScanCamera)
  {
    UE_LOG(SB_Log, Log, TEXT("No Camera given. Please assign one!"));
  }

	// open the socket
	ASpawnBox::LaunchTCP();	

  // Angle to big, reset
  if(Angle >= 360)
  {
    Angle = 90.f;
    UE_LOG(SB_Log, Log, TEXT("This angle is too large. Please select between 0 and 360"));
  }
  // for testing
  SetUpCamera();
  bIsScanning = true;
  GetWorldTimerManager().SetTimer(UpdateTimerHandle, this, &ASpawnBox::UpdateCamera, UpdateTime, true);
}

void ASpawnBox::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );
}

void ASpawnBox::SetUpCamera()
{
	if (ScanCamera)
	{
    // compute new location on a circle around the SpawnVolume
    if( AngleAxis >= 360.0f) { AngleAxis = 0; }

    FVector NewLocation = SpawnVolume->GetComponentLocation();

    FVector RotateValue = FVector(CameraRadius, 0,0).RotateAngleAxis(AngleAxis, FVector::UpVector);

    NewLocation.X += RotateValue.X;
    NewLocation.Y += RotateValue.Y;
    NewLocation.Z += RotateValue.Z;

    // the camera looks to the midpoint of the SpawnVolume
    FRotator Rot = UKismetMathLibrary::FindLookAtRotation(NewLocation, SpawnVolume->GetComponentLocation());

    ScanCamera->SetActorLocationAndRotation(NewLocation, Rot);
	}
}

void ASpawnBox::UpdateCamera()
{
	if (ScanCamera)
	{
    // compute new location on a circle around the SpawnVolume
    AngleAxis += Angle;
    if( AngleAxis >= 360.0f) { AngleAxis = 0; }

    FVector NewLocation = SpawnVolume->GetComponentLocation();

    FVector RotateValue = FVector(CameraRadius, 0,0).RotateAngleAxis(AngleAxis, FVector::UpVector);

    NewLocation.X += RotateValue.X;
    NewLocation.Y += RotateValue.Y;
    NewLocation.Z += RotateValue.Z;

    // the camera looks to the midpoint of the SpawnVolume
    FRotator Rot = UKismetMathLibrary::FindLookAtRotation(NewLocation, SpawnVolume->GetComponentLocation());

    ScanCamera->SetActorLocationAndRotation(NewLocation, Rot);
	}
}

void ASpawnBox::SpawnObjects()
{
	for (auto& Object : SpawnableObjects)
	{
		if (Object) 
		{ 
			AStaticMeshActor* SpawnedActor;
			FVector OriginalLocation = Object->GetActorLocation();

			// increase the max value, if you want more rounds when looking for a possible location
			for (size_t i = 1; i < 10; i++)
			{
				SpawnedActor = SpawnOneObject(Object);
				if (SpawnedActor)
				{
					// spawn in volume successful
					break;
				}
			}

			// spawn wasn't successful, so the Object is set back to default location 
			if (SpawnedActor == nullptr)
			{
				Object->SetActorLocation(OriginalLocation);
				Object->SetActorEnableCollision(true);
				Object->GetStaticMeshComponent()->SetSimulatePhysics(true);
				Object->GetStaticMeshComponent()->SetCollisionProfileName("PhysicsActor");
			}
		}
	}
}

AStaticMeshActor* ASpawnBox::SpawnOneObject(AStaticMeshActor* SpawnThis)
{
	FVector Loc = FindRandomPointInVolume();
	Loc.Z = SpawnVolume->Bounds.Origin.Z;
	FRotator Rot = SpawnThis->GetActorRotation();
		
	// Otherwise location is set based on current location, there mitgh be a better solution, but it works
	SpawnThis->SetActorLocation(FVector::FVector(0, 0, 0));
		
	SpawnThis->SetActorLocationAndRotation(Loc, Rot);
	AStaticMeshActor* SpawnedActor = SpawnThis;

	// check if Spawning was successful 
	if (SpawnedActor)
	{		
		// prep for overlap check and shifting 
		// this also makes it unnecessary to enable all these properties for every item in the editor
		SpawnedActor->SetMobility(EComponentMobility::Movable);
		SpawnedActor->SetActorEnableCollision(false);
		if (SpawnedActor->GetStaticMeshComponent())
		{
			SpawnedActor->GetStaticMeshComponent()->SetSimulatePhysics(true);
			SpawnedActor->GetStaticMeshComponent()->bGenerateOverlapEvents = true;
			SpawnedActor->GetStaticMeshComponent()->SetCollisionProfileName("OverlapAllDynamic");
		}

		// check if there are overlapping actors, ignore the SpawnBox. If true, then shift actor around 
		TArray<AActor*> OverlappingActors;
		SpawnedActor->GetOverlappingActors(OverlappingActors);
		OverlappingActors.Remove(this);
		if (OverlappingActors.Num() > 0)
		{
			// this tries to shift for every overlapping actor 
			for (auto& OverAc : OverlappingActors)
			{
				if (!ShiftObject(SpawnedActor, OverAc))
				{
					return nullptr;
				}
				OverlappingActors.Remove(OverAc);
			}
		}

		// check if all overlaps are resolved (also new overlaps, which are a result of the shifting) 
		// if true, the SpawnedActor has been spawned successfully and can be set to a "solid" simulation state 
		TArray<AActor*> OverlapTest;
		SpawnedActor->GetOverlappingActors(OverlapTest);
		OverlapTest.Remove(this);
		if (OverlappingActors.Num() == OverlapTest.Num())
		{
			SpawnedActor->SetActorEnableCollision(true);
			SpawnedActor->GetStaticMeshComponent()->SetSimulatePhysics(true);
			SpawnedActor->GetStaticMeshComponent()->SetCollisionProfileName("PhysicsActor");
			return SpawnedActor;
		}
		return nullptr;
	}
	return nullptr;
}

bool ASpawnBox::ShiftObject(AStaticMeshActor * ActorToShift, AActor* OverlappingActor)
{
	// check if previous shiftings made this shift obsolete
	if (!ActorToShift->IsOverlappingActor(OverlappingActor))
	{
		return true;
	}
	else
	{	
		// compute a new location to be shifted to and shift
		FVector OverAcOrigin;
		FVector OverAcBox;
		OverlappingActor->GetActorBounds(false, OverAcOrigin, OverAcBox);

		FVector ShiftOrigin;
		FVector ShiftBox;
		ActorToShift->GetActorBounds(false, ShiftOrigin, ShiftBox);
		
		FVector2D Distance = FVector2D(OverAcBox + ShiftBox + FMath::RandRange(MinDistance, MaxDistance));
		int Rand = FMath::RandRange(0, 360); // to make a random point
		Distance.GetRotated(Rand);
		FVector NewLocation = FVector(FVector2D(OverAcOrigin) + Distance, ActorToShift->GetActorLocation().Z);
		ActorToShift->SetActorLocation(NewLocation);

		// check if shifting was succesful and the new Location is valid 
		if (!ActorToShift->IsOverlappingActor(OverlappingActor) && PointIsInVolume(NewLocation))
		{		
			return true;
		}
	}
	return false;
}

	//
	// Utility functions  
	//

bool ASpawnBox::PointIsInVolume(FVector  Point)
{
	FVector VolumeOrigin = SpawnVolume->Bounds.Origin;
	FVector VolumeExtent = SpawnVolume->Bounds.BoxExtent;
	if (VolumeOrigin.X <= (VolumeExtent.X - Point.X)  && VolumeOrigin.Y <= (VolumeExtent.Y - Point.Y) && VolumeOrigin.Z <= (VolumeExtent.Z - Point.Z))
	{
		return true;
	}
	return false;
}

FVector ASpawnBox::FindRandomPointInVolume()
{
	FVector SpawnOrigin = SpawnVolume->Bounds.Origin;
	FVector SpawnExtent = SpawnVolume->Bounds.BoxExtent;
	FVector RandPoint = UKismetMathLibrary::RandomPointInBoundingBox(SpawnOrigin, SpawnExtent);		
	return RandPoint;
}


// Socket / Networking / Communication things

//-----------Start of third Party Code----------------
// The following code was taken from PHi.Wop, who modified Rama's code, especially added a Send function, which is nice to give the client some feedback
// Some small modifications were made
// PHi.Wop Code: https://forums.unrealengine.com/showthread.php?18566-TCP-Socket-Listener-Receiving-Binary-Data-into-UE4-From-a-Python-Script!&p=708455&viewfull=1#post708455
// Rama' Original: https://wiki.unrealengine.com/TCP_Socket_Listener,_Receive_Binary_Data_From_an_IP/Port_Into_UE4,_(Full_Code_Sample)

void ASpawnBox::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  Super::EndPlay(EndPlayReason);

  GetWorld()->GetTimerManager().ClearTimer(TCPConnectionListenerTimerHandle);
  GetWorld()->GetTimerManager().ClearTimer(TCPSocketListenerTimerHandle);
  GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);

  if (ConnectionSocket != NULL) {
    ConnectionSocket->Close();
    UE_LOG(SB_Log, Log, TEXT("ConnectionSocket closed."));
  }
  if (ListenerSocket != NULL) {
    ListenerSocket->Close();
    UE_LOG(SB_Log, Log, TEXT("ListenerSocket closed."));
  }

}

// TCP Server Code
bool ASpawnBox::LaunchTCP()
{
    // change the ip and port to your desired ones
  if (!StartTCPReceiver("RamaSocketListener", IPAddress, Port))
  {
    return false;
  }
  return true;
}

//Rama's Start TCP Receiver
bool ASpawnBox::StartTCPReceiver(
  const FString& YourChosenSocketName,
  const FString& TheIP,
  const int32 ThePort
) {
  //Rama's CreateTCPConnectionListener
  ListenerSocket = CreateTCPConnectionListener(YourChosenSocketName, TheIP, ThePort);

  //Not created?
  if (!ListenerSocket)
  {
    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("StartTCPReceiver>> Listen socket could not be created! ~> %s %d"), *TheIP, ThePort));
    UE_LOG(SB_Log, Error, TEXT("StartTCPReceiver>> Listen socket could not be created!"));
    return false;
  }

  //Start the Listener! //thread this eventually


  UWorld* World = GetWorld();
  World->GetTimerManager().SetTimer(TCPConnectionListenerTimerHandle, this, &ASpawnBox::TCPConnectionListener, 1.0f, true);
  //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("StartTCPReceiver>> Listen socket created")));
  UE_LOG(SB_Log, Log, TEXT("StartTCPReceiver>> Listen socket created"));
  return true;
}
//Format IP String as Number Parts
bool ASpawnBox::FormatIP4ToNumber(const FString& TheIP, uint8(&Out)[4])
{
  //IP Formatting
  TheIP.Replace(TEXT(" "), TEXT(""));

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  //						   IP 4 Parts

  //String Parts
  TArray<FString> Parts;
  TheIP.ParseIntoArray(Parts, TEXT("."), true);
  if (Parts.Num() != 4)
    return false;

  //String to Number Parts
  for (int32 i = 0; i < 4; ++i)
  {
    Out[i] = FCString::Atoi(*Parts[i]);
  }

  return true;
}
//Rama's Create TCP Connection Listener
FSocket* ASpawnBox::CreateTCPConnectionListener(const FString& YourChosenSocketName, const FString& TheIP, const int32 ThePort, const int32 ReceiveBufferSize)
{
  uint8 IP4Nums[4];
  if (!FormatIP4ToNumber(TheIP, IP4Nums))
  {
    UE_LOG(SB_Log, Error, TEXT("IP Address not suitable."));
    return NULL;
  }

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  //Create Socket
  FIPv4Endpoint Endpoint(FIPv4Address(IP4Nums[0], IP4Nums[1], IP4Nums[2], IP4Nums[3]), ThePort);
  FSocket* ListenSocket = FTcpSocketBuilder(*YourChosenSocketName)
    .AsReusable()
    .BoundToEndpoint(Endpoint)
    .Listening(8);

  //Set Buffer Size
  int32 NewSize = 0;
  ListenSocket->SetReceiveBufferSize(ReceiveBufferSize, NewSize);

  //Done!
  return ListenSocket;
}
//Rama's TCP Connection Listener
void ASpawnBox::TCPConnectionListener()
{

  //~~~~~~~~~~~~~
  if (!ListenerSocket) return;
  //~~~~~~~~~~~~~
  //Remote address
  TSharedRef<FInternetAddr> RemoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
  bool Pending;

  // handle incoming connections
  ListenerSocket->HasPendingConnection(Pending);

  if (Pending)
  {
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //Already have a Connection? destroy previous
    if (ConnectionSocket)
    {
      ConnectionSocket->Close();
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket);
    }
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    //New Connection receive!
    ConnectionSocket = ListenerSocket->Accept(*RemoteAddress, TEXT("RamaTCP Received Socket Connection"));
    UE_LOG(SB_Log, Log, TEXT("RamaTCP Received Socket Connection"));

    if (ConnectionSocket != NULL)
    {
      //Global cache of current Remote Address
      RemoteAddressForConnection = FIPv4Endpoint(RemoteAddress);

      //can thread this too
      UWorld* World = GetWorld();

      World->GetTimerManager().SetTimer(TCPSocketListenerTimerHandle, this, &ASpawnBox::TCPSocketListener, 0.1f, true);
    }
  }
}

//Rama's String From Binary Array
FString ASpawnBox::StringFromBinaryArray(TArray<uint8> BinaryArray)
{

  //Create a string from a byte array!
  const std::string cstr(reinterpret_cast<const char*>(BinaryArray.GetData()), BinaryArray.Num());

  return FString(cstr.c_str());

  //BinaryArray.Add(0); // Add 0 termination. Even if the string is already 0-terminated, it doesn't change the results.
  // Create a string from a byte array. The string is expected to be 0 terminated (i.e. a byte set to 0).
  // Use UTF8_TO_TCHAR if needed.
  // If you happen to know the data is UTF-16 (USC2) formatted, you do not need any conversion to begin with.
  // Otherwise you might have to write your own conversion algorithm to convert between multilingual UTF-16 planes.
  //return FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(BinaryArray.GetData())));
}

void ASpawnBox::TCPSend(FString ToSend) {
  ToSend = ToSend + LINE_TERMINATOR; //For Matlab we need a defined line break (fscanf function) "\n" ist not working, therefore use the LINE_TERMINATOR macro form UE

  TCHAR *SerializedChar = ToSend.GetCharArray().GetData();
  int32 Size = FCString::Strlen(SerializedChar);
  int32 Sent = 0;
  uint8* ResultChars = (uint8*)TCHAR_TO_UTF8(SerializedChar);

  if (!ConnectionSocket->Send(ResultChars, Size, Sent)) {
    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Error sending message")));
    UE_LOG(SB_Log, Error, TEXT("Error sending message"));
  }

}

//Rama's TCP Socket Listener
void ASpawnBox::TCPSocketListener()
{
  //~~~~~~~~~~~~~
  if (!ConnectionSocket) return;
  //~~~~~~~~~~~~~


  //Binary Array!
  TArray<uint8> ReceivedData;

  uint32 Size;
  while (ConnectionSocket->HasPendingData(Size))
  {
    ReceivedData.Init(FMath::Min(Size, 65507u), Size);

    int32 Read = 0;
    ConnectionSocket->Recv(ReceivedData.GetData(), ReceivedData.Num(), Read);
  }
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  if (ReceivedData.Num() <= 0)
  {
    return;
  }

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  //						Rama's String From Binary Array
  const FString ReceivedUE4String = StringFromBinaryArray(ReceivedData);
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  // call the InterpretCommand function to make something with the received Strinf command
  InterpretCommand(ReceivedUE4String);
}

//---------End of Third Party Code--------------------------


void ASpawnBox::InterpretCommand(FString cmd)
{
  if (cmd.Equals("StartDefault"))
  {
    CameraHeight = CameraHeight = SpawnVolume->GetComponentLocation().Z;
    RotationSteps = 360;
    CameraRadius = 100;
    ASpawnBox::SpawnObjects();
    SetUpCamera();
    bIsScanning = true;
    ASpawnBox::TCPSend(cmd + " executed!");
    return;
  }
  if (cmd.Equals("Start"))
  {
    SetUpCamera();
    bIsScanning = true;
    ASpawnBox::TCPSend(cmd + " executed!");
    return;
  }
  if (cmd.Equals("Stop"))
  {
    bIsScanning = false;
    ASpawnBox::TCPSend(cmd + " executed!");
    return;
  }
  if (cmd.Equals("Continue"))
  {
    bIsScanning = true;
    ASpawnBox::TCPSend(cmd + " executed!");
    return;
  }
  if (cmd.Equals("Spawn"))
  {
    ASpawnBox::SpawnObjects();
    ASpawnBox::TCPSend(cmd + " executed!");
    return;
  }
  if (cmd.Contains("CameraHeight"))
  {
    FString NumberString;
    cmd.Split("t", nullptr, &NumberString);
    float newValue = FCString::Atof(*NumberString);
    CameraHeight = newValue;
    ASpawnBox::TCPSend(cmd + " executed!");
    return;
  }
  if (cmd.Contains("CameraRadius"))
  {
    FString NumberString;
    cmd.Split("s", nullptr, &NumberString);
    float newValue = FCString::Atof(*NumberString);
    CameraRadius = newValue;
    ASpawnBox::TCPSend(cmd + " executed!");
    return;
  }
  if (cmd.Contains("RotationStep"))
  {
    FString NumberString;
    cmd.Split("p", nullptr, &NumberString);
    float newValue = FCString::Atof(*NumberString);
    RotationSteps = newValue;
    ASpawnBox::TCPSend(cmd + " executed!");
    return;
  }
  if (cmd.Equals("Default"))
  {
    CameraHeight = CameraHeight = SpawnVolume->GetComponentLocation().Z;
    RotationSteps = 360;
    CameraRadius = 100;
    ASpawnBox::TCPSend(cmd + " executed!");
    return;
  }

  // string did not contain a command, visible because of ? at the end
  ASpawnBox::TCPSend("Not a suitable command!");
}
