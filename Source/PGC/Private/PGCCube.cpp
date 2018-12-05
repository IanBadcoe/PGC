// Fill out your copyright notice in the Description page of Project Settings.

#include "PGCCube.h"


// Sets default values
FPGCCube::FPGCCube()
	: X{ 0 }, Y{ 0 }, Z{ 0 },
	  Sharp {
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
