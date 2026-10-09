// Ghidra headless script: decompile the functions at the given addresses with a long timeout.
// Usage: analyzeHeadless <proj> <name> -process desperados32 -noanalysis -postScript DecompOne.java <out.c> <hexaddr>...
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class DecompOne extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        DecompInterface ifc = new DecompInterface();
        DecompileOptions opt = new DecompileOptions();
        opt.setMaxPayloadMBytes(512);
        ifc.setOptions(opt);
        ifc.openProgram(currentProgram);
        PrintWriter pw = new PrintWriter(new BufferedWriter(new FileWriter(a[0])));
        for (int i = 1; i < a.length; ++i) {
            Function f = getFunctionAt(toAddr(a[i]));
            pw.println("// ==== " + f.getName(true) + " @ " + f.getEntryPoint());
            DecompileResults r = ifc.decompileFunction(f, 3000, monitor);
            if (r != null && r.decompileCompleted()) pw.println(r.getDecompiledFunction().getC());
            else pw.println("// decompile failed: " + (r == null ? "" : r.getErrorMessage()));
        }
        pw.close();
    }
}
