// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Object.h"
#include "GameFramework/Actor.h"
#include "Networking.h"
#include "Classes/Components/BoxComponent.h"
#include "Camera/CameraActor.h"
#include "URoboVision/Public/RGBDCamera.h"
#include "RSpawnBox.generated.h"

	// Log for this Class
	DECLARE_LOG_CATEGORY_EXTERN(SB_Log, Log, All);

UCLASS()
class ASpawnBox : public AActor
{
	GENERATED_BODY()
	
public:	
	/**
	 * Sets default values for this actor's properties
	 */
	ASpawnBox();

	/**
	 * Called when the game starts or when spawned
	 */
	virtual void BeginPlay() override;
	
	/** 
	 * Called every frame
	 */
	virtual void Tick( float DeltaSeconds ) override;

	/** The Volume in which the objects will be spawned*/
	UPROPERTY(EditAnywhere)
		UBoxComponent* SpawnVolume;

	/** StaticMeshActors held in this Array will be spawned at random locations in the SpawnVolume */
	UPROPERTY(EditAnywhere, Category = "Spawn Settings")
		TArray<AStaticMeshActor*> SpawnableObjects;

	/** The Minimum Distance between two objects */
	UPROPERTY(EditAnywhere, Category = "Spawn Settings")
		float MinDistance;

	/** The Maximum Distance between two objects */
	UPROPERTY(EditAnywhere, Category = "Spawn Settings")
		float MaxDistance;
	
  /** The Camera used for scanning the scene */
  UPROPERTY(EditAnywhere, Category = "Camera Settings")
  ARGBDCamera* ScanCamera;

	/** The Radius, on which the Camera should rotate around the SpawnBox */
	UPROPERTY(EditAnywhere, Category = "Camera Settings")
		int CameraRadius;

	/** The Height of the Camera */
	UPROPERTY(EditAnywhere, Category = "Camera Settings")
		int CameraHeight;

  /** The angle the camera will be rotated around the SpawnVolume per rotation */
  UPROPERTY(EditAnywhere, Category = "Scan Settings")
    float Angle;

  UPROPERTY(EditAnywhere, Category = "Scan Settings")
    float UpdateTime;

	/** The IP Adsress for the server socket */
	UPROPERTY(EditAnywhere, Category = "TCPSocket")
		FString IPAddress;

	/** The port for the server socket */
	UPROPERTY(EditAnywhere, Category = "TCPSocket")
		int Port;

  /**
   * Udates the position of the ScanCamera based on the given rotation steps
   */
  UFUNCTION()
  void UpdateCamera();

	/**
	 * Spawns the objects in the Array SpawnableObjects
	 */
	UFUNCTION()
		void SpawnObjects();


	/*-----------Start of third Party Code----------------
	* The following code was taken from PHi.Wop, who modified Rama's code, especially added a Send function, which is nice to give 
	* PHi.Wop Code: https://forums.unrealengine.com/showthread.php?18566-TCP-Socket-Listener-Receiving-Binary-Data-into-UE4-From-a-Python-Script!&p=708455&viewfull=1#post708455
	* Rama' Original: https://wiki.unrealengine.com/TCP_Socket_Listener,_Receive_Binary_Data_From_an_IP/Port_Into_UE4,_(Full_Code_Sample)
	*/

  FSocket* ListenerSocket;
  FSocket* ConnectionSocket;
  FIPv4Endpoint RemoteAddressForConnection;

  FTimerHandle TCPSocketListenerTimerHandle;
  FTimerHandle TCPConnectionListenerTimerHandle;

  bool StartTCPReceiver(
    const FString& YourChosenSocketName,
    const FString& TheIP,
    const int32 ThePort
  );

  FSocket* CreateTCPConnectionListener(
    const FString& YourChosenSocketName,
    const FString& TheIP,
    const int32 ThePort,
    const int32 ReceiveBufferSize = 2 * 1024 * 1024
  );

  //Timer functions, could be threads
  void TCPConnectionListener(); 	//can thread this eventually
  FString StringFromBinaryArray(TArray<uint8> BinaryArray);
  void TCPSocketListener();		//can thread this eventually


  //Format String IP4 to number array
  bool FormatIP4ToNumber(const FString& TheIP, uint8(&Out)[4]);

  /**
  * Launches the TCP Socket
  *
  * @return true is lauch was successful
  */
  UFUNCTION(BlueprintCallable, Category = "TCPServer")
    bool LaunchTCP();

  /**
  * Sends the given string back to the client
  *
  * @param message the string to send
  */
  UFUNCTION(BlueprintCallable, Category = "TCPServer")
    void TCPSend(FString message);

  /* not used */
  //UFUNCTION(BlueprintImplementableEvent, Category = "TCPServer")
  //	void recievedMessage(const FString &message);



protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;



private:

	/** true, when the Camera (should) scan */
	bool bIsScanning;

  float AngleAxis;

  bool bIsWaitingForTimer;

  FTimerHandle UpdateTimerHandle;

	/**
	 * Set the ScanCamera to the Location specified through CameraRadius and CameraHeight
	 * Also sets PreviousHypo the first time
	 */
	void SetUpCamera();


	/**
  	 * Spawns one object and returns it
	 *
	 * @param SpawnThis the object to spawn
	 * @return the actor that was spawned
	 */
	UFUNCTION()
		AStaticMeshActor* SpawnOneObject(AStaticMeshActor* SpawnThis);

	/**
	 * Shifts the given Actor to a location, where it is not overlapping the OverlappingActor
	 * If shift was successful, returns true
	 *
	 * @param ActorToShift the actor to be shifted
	 * @param OverlappingActor the actor that overlaps the ActorToShift
	 * @return true if the shift was successful, so ActorToShift and OverlappingActor do not overlap anymore
	 */
	UFUNCTION()
		bool ShiftObject(AStaticMeshActor* ActorToShift, AActor* OverlappingActor);

	/**
	* Checks if the given String is a possible command and executes the coressponding logic
	*
	* @param cmd the cmd string with a possible command
	*/
	UFUNCTION()
		void InterpretCommand(FString cmd);


	//
	// Utility Functions
	//

	/**
	 * Checks if a point is in the SpawnVolume
	 *
	 * @param Point the vector of the point to be checked
	 * @return true if the point is in the SpawnVolume
	 */
	UFUNCTION()
		bool PointIsInVolume(FVector Point);

	/**
	* Computes a random point in the SpawnVolume and returns that point
	*
	* @return a random point in the SpawnVolume
	*/
	UFUNCTION()
		FVector FindRandomPointInVolume();
};
