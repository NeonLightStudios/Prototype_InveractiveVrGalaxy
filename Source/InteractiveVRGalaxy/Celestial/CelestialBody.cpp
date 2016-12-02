// Fill out your copyright notice in the Description page of Project Settings.

#include "InteractiveVRGalaxy.h"
#include "CelestialBody.h"

#define ORBIT_BEAM_SCALE_MULTIPLIER 1.25f
#define GRAVITATIONAL_CONSTANT 6.67408e-11

#define SPHERE_MESH_LOCATION TEXT("StaticMesh'/Game/VirtualRealityBP/Blueprints/Planets/SphereMesh.SphereMesh'")

// Sets default values
ACelestialBody::ACelestialBody() : m_Material(nullptr), m_MaterialDynamic(nullptr), m_Atmosphere(nullptr), m_Orbit(nullptr)
{
	// Atmosphere
	this->m_bDrawAtmosphere = false;

	// Draw Orbit
	this->m_bDrawOrbit = false;
	this->m_DrawOrbitRadius = 40.0f;
	this->m_DrawOrbitResolution = 50;
	this->m_DrawOrbitColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.25f);

	// Orbit
	this->m_CurrentSpeed = 0.0f;
	this->m_Angle = 0.0f;
	this->m_RotateOrbitClockwise = true;

	// Scale
	this->m_VelocityScale = 1.0f;
	this->m_RotationScale = 1.0f;
	this->m_RadiusScale = 1.0f;
	this->m_OrbitDistanceScale = 1.0f;

	// Movement
	this->m_bMoveBody = true;
	this->m_bMoveBodyTransition = false;
	this->m_TransitionDelay = 1.0f;

	// World
	this->m_SunLocation = FVector(0.0f);

	// Other
	this->m_Root = UObject::CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyRoot"));
	if(this->m_Material)
	{
		this->m_Root->SetMaterial(0, this->m_Material);
	}
	Super::RootComponent = this->m_Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> sphere(SPHERE_MESH_LOCATION);
	if (sphere.Succeeded())
	{
		this->m_Root->SetStaticMesh(sphere.Object);
	}
}

void ACelestialBody::BeginPlay()
{
	this->CalculateSemiMinorAxis();
	this->CalculatePerimeter();

	// Check to see if this planet has been spawned without a solar system
	if(Super::GetAttachParentActor() == nullptr)
	{
		this->SetScale(1.0f);
	}
	this->m_MaterialDynamic = UMaterialInstanceDynamic::Create(this->m_Material, this->m_Root);

	this->SetSunLocation(this->m_SunLocation);
	if(this->m_bDrawOrbit)
	{
		this->m_bDrawOrbit = false;
		this->SetDrawOrbit(true);
	}
	if(this->m_bDrawAtmosphere)
	{
		this->m_bDrawAtmosphere = false;
		this->SetDrawAtmosphere(true);
	}
	this->m_Root->SetRelativeRotation(FQuat(FVector(0.0f, 1.0f, 0.0f), -this->m_AxialTilt * DEG_TO_RAD));
}

#if WITH_EDITOR
void ACelestialBody::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// If the Semi-major Axis or Eccentricity changes we will need to recalculate the Semi-minor axis
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_SemiMajorAxis)
		|| name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_Eccentricity))
	{
		this->CalculateSemiMinorAxis();
		this->CalculatePerimeter();
	}
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_Material))
	{
		this->m_MaterialDynamic = UMaterialInstanceDynamic::Create(this->m_Material, this->m_Root);
		// Update material parameters
		this->SetSunLocation(this->m_SunLocation);
	}
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_AxialTilt))
	{
		this->m_Root->SetRelativeRotation(FQuat(FVector(0.0f, 1.0f, 0.0f), -this->m_AxialTilt * DEG_TO_RAD));
	}
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_bDrawOrbit))
	{
		this->m_bDrawOrbit = !this->m_bDrawOrbit;
		this->SetDrawOrbit(!this->m_bDrawOrbit);
	}
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_bDrawAtmosphere))
	{
		this->m_bDrawAtmosphere = !this->m_bDrawAtmosphere;
		this->SetDrawAtmosphere(!this->m_bDrawAtmosphere);
	}
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_RadiusScale))
	{
		this->SetRadiusScale(this->m_RadiusScale);
	}
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_DrawOrbitRadius) && this->m_Orbit != nullptr)
	{
		this->m_Orbit->SetRadius(this->m_DrawOrbitRadius);
		this->m_Orbit->UpdateOrbit();
	}
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_DrawOrbitColor) && this->m_Orbit != nullptr)
	{
		this->m_Orbit->SetColor(this->m_DrawOrbitColor);
		this->m_Orbit->UpdateOrbit();
	}
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_DrawOrbitResolution) && this->m_Orbit != nullptr)
	{
		this->SetDrawOrbit(false);
		this->SetDrawOrbit(true);
	}
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_bMoveBody))
	{
		this->SetMoveBody(this->m_bMoveBody);
	}
	if (name == GET_MEMBER_NAME_CHECKED(ACelestialBody, m_SunLocation))
	{
		this->SetSunLocation(this->m_SunLocation);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void ACelestialBody::SetSunLocation(const FVector& location)
{
	this->m_SunLocation = location;
	
	if(this->m_MaterialDynamic)
	{
		this->m_MaterialDynamic->SetVectorParameterValue(MATERIAL_NAME_SUN_LOCATION, this->m_SunLocation);
		this->m_Root->SetMaterial(0, this->m_MaterialDynamic);
	}
	if(this->m_Atmosphere != nullptr)
	{
		this->m_Atmosphere->UpdateAtmosphere();
	}
}

void ACelestialBody::SetScale(const float& scale)
{
	check(this->m_Root);
	float newScale = this->m_Radius * scale * this->m_RadiusScale;
	this->m_Root->SetWorldScale3D(FVector(newScale));
	this->m_LastRadiusScale = scale;
	if(this->m_Atmosphere != nullptr)
	{
		this->m_Atmosphere->UpdateAtmosphere();
	}
}

float ACelestialBody::CalculateRotation(const float& radians) const
{
	float rotation = FMath::Fmod(this->m_OrbitPeriod * this->m_RotationScale * (radians / PI2), 1.0f) * PI2;
	return this->m_RotatePlanetClockwise ? rotation : -rotation;
}

float ACelestialBody::CalculateVelocity(const float& radians) const
{
	return (this->m_MinSpeed + (this->m_MaxSpeed - this->m_MinSpeed) * FMath::Sin(radians / 2.0f)) * this->m_VelocityScale;
}

float ACelestialBody::CalculateDistance(const float& radians) const
{
	//r = (a(1-e^2))/(1+ecosR)
	return (this->m_SemiMajorAxis * (1.0f - this->m_Eccentricity * this->m_Eccentricity)) / (1.0f + this->m_Eccentricity * FMath::Cos(radians));
}

FVector ACelestialBody::CalculatePosition(const float& radians, const float& offset, const float& distanceScale) const
{
	FVector vector;
	vector.X = (offset + this->m_SemiMajorAxis * distanceScale * this->m_OrbitDistanceScale) * -FMath::Cos(radians);
	vector.Y = (offset + this->m_SemiMinorAxis * distanceScale * this->m_OrbitDistanceScale) * FMath::Sin(radians);
	vector.Z = 0.0f;
	if (!this->m_RotateOrbitClockwise)
	{
		vector.Y = -vector.Y;
	}
	return Super::GetAttachParentActor()->GetActorLocation() + vector;
}

void ACelestialBody::SetDrawAtmosphere(const bool& draw)
{
	if(this->m_bDrawAtmosphere == draw)
	{
		return;
	}
	this->m_bDrawAtmosphere = draw;
	if(Super::GetWorld() == nullptr)
	{
		return;
	}
	if(!draw)
	{
		if(this->m_Atmosphere != nullptr)
		{
			this->m_Atmosphere->Destroy();
			this->m_Atmosphere = nullptr;
		}
	}
	else
	{
		this->m_Atmosphere = (AAtmosphere*)Super::GetWorld()->SpawnActor(AAtmosphere::StaticClass());
		if(this->m_Atmosphere)
		{
			this->m_Atmosphere->SetAtmosphereData(&this->m_AtmosphereData);
			this->m_Atmosphere->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

			this->m_Atmosphere->UpdateAtmosphere();
		}
	}
}

void ACelestialBody::SetDrawOrbit(const bool& draw)
{
	if (this->m_bDrawOrbit == draw)
	{
		return;
	}
	this->m_bDrawOrbit = draw;
	if (Super::GetWorld() == nullptr)
	{
		return;
	}
	if (!draw)
	{
		if (this->m_Orbit != nullptr)
		{
			this->m_Orbit->Destroy();
			this->m_Orbit = nullptr;
		}
	}
	else
	{
		AActor *parent = Super::GetAttachParentActor();
		if(parent == nullptr)
		{
			this->m_bDrawOrbit = false;
			return;
		}
		this->m_Orbit = (AOrbit*)Super::GetWorld()->SpawnActor(AOrbit::StaticClass());
		if (this->m_Orbit)
		{
			TArray<FVector> points;
			for (int i = 0; i < this->m_DrawOrbitResolution; i++)
			{	
				float delta = PI2 * (i + 1) / this->m_DrawOrbitResolution;
				points.Add(this->CalculatePosition(delta, this->m_LastOffset, this->m_LastDistanceScale) - parent->GetActorLocation());
			}
			this->m_Orbit->SetPoints(points);
			this->m_Orbit->SetColor(this->m_DrawOrbitColor);
			this->m_Orbit->SetRadius(this->m_DrawOrbitRadius);
			this->m_Orbit->AttachToActor(parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

			this->m_Orbit->UpdateOrbit();
		}
	}
}

//void ACelestialBody::SetDrawOrbit(const bool& draw)
//{
//	if(this->m_bDrawOrbit == draw || (draw && this->m_ParticleSystem == nullptr))
//	{
//		return;
//	}
//	this->m_bDrawOrbit = draw;
//	if (Super::GetWorld() == nullptr)
//	{
//		return;
//	}
//	if(!draw)
//	{
//		for(int i = 0; i < this->m_OrbitParticleSystems.Num(); i++)
//		{
//			this->m_OrbitParticleSystems[i]->DestroyComponent();
//		}
//		this->m_OrbitParticleSystems.Empty();
//	}
//	else
//	{
//		check(this->m_OrbitParticleResolution >= 0);
//
//		FVector prevLoc = this->m_Root->GetComponentLocation();
//		for(int i = 0; i < this->m_OrbitParticleResolution; i++)
//		{
//			UParticleSystemComponent *system = UGameplayStatics::SpawnEmitterAtLocation(Super::GetWorld(),
//				this->m_ParticleSystem, this->m_Root->GetComponentLocation(), FRotator::ZeroRotator, false);
//			if(system == nullptr)
//			{
//				continue;
//			}
//			float currentRad = PI2 * (i + 1) / this->m_OrbitParticleResolution;
//			FVector target = this->CalculatePosition(currentRad, this->m_LastOffset, this->m_LastDistanceScale);
//			system->SetBeamSourcePoint(0, prevLoc, 0);
//			system->SetBeamTargetPoint(0, prevLoc = target, 0);
//
//			system->SetColorParameter(FName("InitialColor"), this->m_OrbitColor);
//			// Set the scale of the beam to be equal to the bodies scale. Has an additional multiplier to increase or decrease the scale using the bodies as a base.
//			system->SetWorldScale3D(FVector(ORBIT_BEAM_SCALE_MULTIPLIER * this->GetRadiusWithScale() * 2.0f));
//
//			this->m_OrbitParticleSystems.Add(system);
//		}
//	}
//}

void ACelestialBody::Move(const ACelestialBody *center, const float& timeScale, const float& distanceScale, const float& delta)
{
	check(this->m_Root);
	float velocity = this->CalculateVelocity(this->GetAngleToCenter() * DEG_TO_RAD);
	this->m_CurrentSpeed = velocity;

	float kmPerDegree = this->m_Perimeter * distanceScale * this->m_OrbitDistanceScale / 360.0f;
	check(kmPerDegree != 0.0f);

	float offset;
	if (Super::GetAttachParentActor()->IsA(ACelestialBody::StaticClass()))
	{
		offset = ((ACelestialBody*)Super::GetAttachParentActor())->GetRadiusWithScale();
	}
	else
	{
		offset = center->GetRadiusWithScale();
	}
	this->m_LastOffset = offset;
	this->m_LastDistanceScale = distanceScale;

	this->m_Angle += (velocity * distanceScale * this->m_OrbitDistanceScale * timeScale * delta / kmPerDegree);
	this->m_Angle = FMath::Fmod(this->m_Angle, 360.0f);

	this->m_PlanetRotation = this->CalculateRotation(this->m_Angle * DEG_TO_RAD) * RAD_TO_DEG;

	if(this->m_bMoveBody)
	{
		FVector targetPosition = this->CalculatePosition(this->m_Angle * DEG_TO_RAD, offset, distanceScale);
		FRotator targetRotation = FRotator(0.0f, this->m_PlanetRotation, 0.0f);
		if(this->m_bMoveBodyTransition)
		{
			this->m_TransitionTimer += delta;
			if(this->m_TransitionTimer >= this->m_TransitionDelay)
			{
				this->m_bMoveBodyTransition = false;
				this->m_TransitionTimer = this->m_TransitionDelay;
			}
			float perc = FMath::Sin(this->m_TransitionTimer / this->m_TransitionDelay * PI / 2.0f);
			this->m_Root->SetWorldLocation(FMath::Lerp(this->m_TransitionPosStart, targetPosition, perc));
			this->m_Root->SetRelativeRotation(FMath::Lerp(this->m_TransitionRotStart, targetRotation, perc));
		}
		else
		{
			this->m_Root->SetRelativeRotation(targetRotation);
			this->m_Root->SetWorldLocation(targetPosition);
		}
	}
}