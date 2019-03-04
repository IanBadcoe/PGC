// Fill out your copyright notice in the Description page of Project Settings.

#include "PGCCube.h"


// Sets default values
FPGCCube::FPGCCube()
	: X{ 0 }, Y{ 0 }, Z{ 0 },
	  EdgeTypes {
		PGCEdgeType::Rounded, PGCEdgeType::Rounded, PGCEdgeType::Rounded, PGCEdgeType::Rounded,
		PGCEdgeType::Rounded, PGCEdgeType::Rounded, PGCEdgeType::Rounded, PGCEdgeType::Rounded,
		PGCEdgeType::Rounded, PGCEdgeType::Rounded, PGCEdgeType::Rounded, PGCEdgeType::Rounded
	  }
{
}

FPGCCube::FPGCCube(int x, int y, int z)
	: FPGCCube()
{
	X = x;
	Y = y;
	Z = z;
}

uint32 FPGCCube::GetTypeHash() const
{
	auto ret = HashCombine(::GetTypeHash(X), ::GetTypeHash(Y));

	ret = HashCombine(ret, ::GetTypeHash(Z));

	for (const auto& et : EdgeTypes)
	{
		ret = HashCombine(ret, ::GetTypeHash(et));
	}

	return ret;
}
