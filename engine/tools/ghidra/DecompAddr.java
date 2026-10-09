// Ghidra headless script: decompile functions, each C line prefixed with the lowest instruction
// address of its tokens (to find a line's call in the disassembly, where Ghidra lost arguments).
// Usage: ... -postScript DecompAddr.java <out.c> <hexaddr>...
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.decompiler.component.DecompilerUtils;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import java.io.*;
import java.util.*;

public class DecompAddr extends GhidraScript {
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
            if (r == null || !r.decompileCompleted()) { pw.println("// decompile failed"); continue; }
            ClangTokenGroup markup = r.getCCodeMarkup();
            List<ClangLine> lines = DecompilerUtils.toLines(markup);
            for (ClangLine line : lines) {
                Address min = null;
                StringBuilder sb = new StringBuilder();
                for (ClangToken t : line.getAllTokens()) {
                    Address ad = t.getMinAddress();
                    if (ad != null && (min == null || ad.compareTo(min) < 0)) min = ad;
                    sb.append(t.getText());
                }
                String ind = "  ".repeat(line.getIndent() / 2 > 0 ? line.getIndent() / 2 : 0);
                pw.println((min == null ? "        " : min.toString().substring(min.toString().length() - 8)) + " " + ind + sb);
            }
        }
        pw.close();
    }
}
