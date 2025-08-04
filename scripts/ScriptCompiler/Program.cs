using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;

if (args.Length < 2)
{
    Console.WriteLine("Usage: ScriptCompiler <inputDir> <outputDll> [references...]");
    return;
}

Console.WriteLine("Args:");
for (int i = 0; i < args.Length; i++)
    Console.WriteLine($"  [{i}] = \"{args[i]}\"");


string inputDir = args[0];
string outputDll = args[1];
var extraRefs = args.Skip(2).ToList();

var files = Directory.GetFiles(inputDir, "*.cs", SearchOption.AllDirectories);
if (files.Length == 0)
{
    Console.WriteLine("No .cs files found.");
    return;
}

// Syntax trees
var syntaxTrees = files.Select(file => CSharpSyntaxTree.ParseText(File.ReadAllText(file), path: file)).ToList();

// Default core references
var trustedAssemblies = ((string?)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES"))?
    .Split(Path.PathSeparator)
    .Where(p => p.Contains("System.") || p.EndsWith("mscorlib.dll") || p.EndsWith("netstandard.dll"))
    .Select(p => MetadataReference.CreateFromFile(p))
    .ToList() ?? new();

// Add custom references
foreach (var path in extraRefs)
{
    if (!File.Exists(path))
    {
        Console.WriteLine($"[Warning] Reference not found: {path}");
        continue;
    }
    trustedAssemblies.Add(MetadataReference.CreateFromFile(path));
}

// Compilation
var compilation = CSharpCompilation.Create(Path.GetFileNameWithoutExtension(outputDll))
    .WithOptions(new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary))
    .AddReferences(trustedAssemblies)
    .AddSyntaxTrees(syntaxTrees);

// Emit
var emitResult = compilation.Emit(outputDll);

if (!emitResult.Success)
{
    Console.ForegroundColor = ConsoleColor.Red;
    foreach (var diag in emitResult.Diagnostics)
        Console.WriteLine(diag.ToString());
    Console.ResetColor();
    Environment.Exit(1);
}
else
{
    Console.ForegroundColor = ConsoleColor.Green;
    Console.WriteLine($"✅ Compiled {files.Length} scripts to {outputDll}");
    Console.ResetColor();
}
