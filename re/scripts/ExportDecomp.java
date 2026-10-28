// Ghidra headless script: export decompiled C for every function,
// plus a function list and the defined strings.
//
// Usage: analyzeHeadless <projdir> <projname> -import Dasmx.exe \
//            -scriptPath re/scripts -postScript ExportDecomp.java <outdir>
//@category Export

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.io.File;
import java.io.PrintWriter;

public class ExportDecomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File outDir = new File(args.length > 0 ? args[0] : "out");
        outDir.mkdirs();

        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        try (PrintWriter all = new PrintWriter(new File(outDir, "decompiled.c"));
             PrintWriter list = new PrintWriter(new File(outDir, "functions.txt"))) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Function f = it.next();
                list.printf("%s\t%s\t%d%n", f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses());
                DecompileResults res = decomp.decompileFunction(f, 120, monitor);
                all.printf("%n// ==== %s @ %s ====%n", f.getName(), f.getEntryPoint());
                if (res.decompileCompleted()) {
                    all.println(res.getDecompiledFunction().getC());
                } else {
                    all.println("// decompilation failed: " + res.getErrorMessage());
                }
            }
        }

        try (PrintWriter strs = new PrintWriter(new File(outDir, "strings.txt"))) {
            DataIterator di = currentProgram.getListing().getDefinedData(true);
            while (di.hasNext()) {
                Data d = di.next();
                if (d.hasStringValue()) {
                    strs.printf("%s\t%s%n", d.getAddress(), d.getDefaultValueRepresentation());
                }
            }
        }
        decomp.dispose();
    }
}
