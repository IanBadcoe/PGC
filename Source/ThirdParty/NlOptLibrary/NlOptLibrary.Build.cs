// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.IO;
using UnrealBuildTool;

public class NlOptLibrary : ModuleRules
{
    public string GetUProjectPath()
    {
        //Change this according to your module's relative location to your project file. If there is any better way to do this I'm interested!
        //Currently Plugins/PGC/Source/ThirdParty/NlOptLibrary/
        return new DirectoryInfo(Path.GetFullPath(ModuleDirectory)).Parent.Parent.Parent.Parent.Parent.FullName;
    }

    private void CopyToBinaries(string Filepath, ReadOnlyTargetRules Target)
    {
        string binariesDir = Path.Combine(GetUProjectPath(), "Binaries", Target.Platform.ToString());
        string filename = Path.GetFileName(Filepath);
//        Log.TraceInformation(String.Format("Copying DLL {0} -> {1}", Filepath, Path.Combine(binariesDir, filename)));

        if (!Directory.Exists(binariesDir))
        {
            Directory.CreateDirectory(binariesDir);
        }

        if (!File.Exists(Path.Combine(binariesDir, filename)))
        {
            File.Copy(Filepath, Path.Combine(binariesDir, filename), true);
        }
    }

    public NlOptLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            PublicDefinitions.Add("WITH_NLOPT=1");

            // Compile and link with YOURLIB

            PublicIncludePaths.Add(ModuleDirectory + "/nlopt");
            PublicAdditionalLibraries.Add(ModuleDirectory + "/nlopt/libnlopt-0.lib");
            CopyToBinaries(ModuleDirectory + "/nlopt/libnlopt-0.dll", Target);
  
			// Delay-load the DLL, so we can load it from the right place first
			PublicDelayLoadDLLs.Add("libnlopt-0.dll");
		}
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            System.Diagnostics.Debug.Assert(false);
//            PublicDelayLoadDLLs.Add(Path.Combine(ModuleDirectory, "Mac", "Release", "libExampleLibrary.dylib"));
        }
	}
}
