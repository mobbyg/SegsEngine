using GodotTools.Core;
using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using GodotTools.BuildLogger;
using GodotTools.Internals;
using GodotTools.Utils;
using Directory = System.IO.Directory;

namespace GodotTools.Build
{
    public static class BuildSystem
    {
        private static string MonoWindowsBinDir
        {
            get
            {
                string monoWinBinDir = Path.Combine(Internal.MonoWindowsInstallRoot, "bin");

                if (!Directory.Exists(monoWinBinDir))
                    throw new FileNotFoundException("Cannot find the Windows Mono install bin directory.");

                return monoWinBinDir;
            }
        }

        private static Godot.EditorSettings EditorSettings =>
            GodotSharpEditor.Instance.GetEditorInterface().GetEditorSettings();

        private static Process LaunchBuild(BuildInfo buildInfo, Action<string> stdOutHandler, Action<string> stdErrHandler)
        {
            (string msbuildPath, BuildTool buildTool) = MsBuildFinder.FindMsBuild();

            if (msbuildPath == null)
                throw new FileNotFoundException("Cannot find the MSBuild executable.");

            string compilerArgs = BuildArguments(buildTool, buildInfo);

            var startInfo = new ProcessStartInfo(msbuildPath, compilerArgs);

            string launchMessage = $"Running: \"{startInfo.FileName}\" {startInfo.Arguments}";
            stdOutHandler?.Invoke(launchMessage);
            if (Godot.OS.IsStdoutVerbose())
                Console.WriteLine(launchMessage);

            startInfo.RedirectStandardOutput = true;
            startInfo.RedirectStandardError = true;
            startInfo.UseShellExecute = false;
            startInfo.CreateNoWindow = true;

            // Needed when running from Developer Command Prompt for VS
            RemovePlatformVariable(startInfo.EnvironmentVariables);
            startInfo.EnvironmentVariables["DOTNET_SKIP_FIRST_TIME_EXPERIENCE"] = "1";

            var process = new Process {StartInfo = startInfo};

            if (stdOutHandler != null)
                process.OutputDataReceived += (s, e) => stdOutHandler.Invoke(e.Data);
            if (stdErrHandler != null)
                process.ErrorDataReceived += (s, e) => stdErrHandler.Invoke(e.Data);

            process.Start();

            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            return process;
        }

        public static int Build(BuildInfo buildInfo, Action<string> stdOutHandler, Action<string> stdErrHandler)
        {
            using (var process = LaunchBuild(buildInfo, stdOutHandler, stdErrHandler))
            {
                process.WaitForExit(32000);

                return process.ExitCode;
            }
        }

        public static async Task<int> BuildAsync(BuildInfo buildInfo, Action<string> stdOutHandler, Action<string> stdErrHandler)
        {
            using (var process = LaunchBuild(buildInfo, stdOutHandler, stdErrHandler))
            {
                await process.WaitForExitAsync();

                return process.ExitCode;
            }
        }

        private static string BuildArguments(BuildTool buildTool, BuildInfo buildInfo)
        {
            string arguments = string.Empty;

            if (buildTool == BuildTool.DotnetCli)
                arguments += "msbuild "; // `dotnet msbuild` command

            arguments += $@" ""{buildInfo.Solution}""";

            if (buildInfo.Restore)
                arguments += " /restore";

            arguments += $@" /t:{string.Join(",", buildInfo.Targets)} " +
                         $@"""/p:{"Configuration=" + buildInfo.Configuration}"" /v:normal " +
                         $@"""/l:{typeof(GodotBuildLogger).FullName},{GodotBuildLogger.AssemblyPath};{buildInfo.LogsDirPath}""";

            foreach (string customProperty in buildInfo.CustomProperties)
            {
                arguments += " /p:" + customProperty;
            }

            return arguments;
        }

        private static void RemovePlatformVariable(StringDictionary environmentVariables)
        {
            // EnvironmentVariables is case sensitive? Seriously?

            var platformEnvironmentVariables = new List<string>();

            foreach (string env in environmentVariables.Keys)
            {
                if (env.ToUpper() == "PLATFORM")
                    platformEnvironmentVariables.Add(env);
            }

            foreach (string env in platformEnvironmentVariables)
                environmentVariables.Remove(env);
        }
    }
}
