#!/usr/bin/env python3
"""The window of the OpenCMR2 CIA Builder (the work is in cia_builder.py).

Windows: double-click it (Python 3 from python.org includes the window
toolkit). Linux: python3 cia_builder_gui.py, with Tk installed
(CachyOS/Arch: pacman -S tk; Debian/Ubuntu: apt install python3-tk).
"""

import queue
import sys
import threading
import traceback
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

import cia_builder as builder


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("OpenCMR2 CIA Builder")
        self.minsize(640, 460)
        self.install = None
        self.events = queue.Queue()
        self.busy = False

        self.exe = tk.StringVar()
        self.mode = tk.StringVar(value="cia")
        self.output = tk.StringVar()
        self.kit = tk.StringVar(value=str(builder.default_kit()))

        pad = {"padx": 8, "pady": 4}
        frame = ttk.Frame(self, padding=10)
        frame.pack(fill="both", expand=True)
        frame.columnconfigure(1, weight=1)

        ttk.Label(frame, text="Game (CMR2.exe):").grid(row=0, column=0, sticky="w", **pad)
        exe_entry = ttk.Entry(frame, textvariable=self.exe)
        exe_entry.grid(row=0, column=1, sticky="ew", **pad)
        exe_entry.bind("<Return>", lambda event: self.check())
        exe_entry.bind("<FocusOut>", lambda event: self.check() if self.exe.get() else None)
        ttk.Button(frame, text="Browse...", command=self.pick_exe).grid(row=0, column=2, **pad)

        self.status = ttk.Label(frame, text="Pick the CMR2.exe of your PC installation.", justify="left")
        self.status.grid(row=1, column=0, columnspan=3, sticky="w", **pad)

        modes = ttk.LabelFrame(frame, text="Output", padding=6)
        modes.grid(row=2, column=0, columnspan=3, sticky="ew", **pad)
        ttk.Radiobutton(modes, text="Standalone CIA (game files inside; install with FBI)", value="cia",
                        variable=self.mode, command=self.mode_changed).pack(anchor="w")
        ttk.Radiobutton(modes, text="SD card folder (sd:/3ds/OpenCMR2/Data/, for the plain CIA)", value="sd",
                        variable=self.mode, command=self.mode_changed).pack(anchor="w")

        self.output_label = ttk.Label(frame, text="CIA file:")
        self.output_label.grid(row=3, column=0, sticky="w", **pad)
        ttk.Entry(frame, textvariable=self.output).grid(row=3, column=1, sticky="ew", **pad)
        ttk.Button(frame, text="Browse...", command=self.pick_output).grid(row=3, column=2, **pad)

        self.kit_label = ttk.Label(frame, text="Kit:")
        self.kit_label.grid(row=4, column=0, sticky="w", **pad)
        self.kit_entry = ttk.Entry(frame, textvariable=self.kit)
        self.kit_entry.grid(row=4, column=1, sticky="ew", **pad)
        self.kit_button = ttk.Button(frame, text="Browse...", command=self.pick_kit)
        self.kit_button.grid(row=4, column=2, **pad)

        self.build_button = ttk.Button(frame, text="Build", command=self.build, state="disabled")
        self.build_button.grid(row=5, column=0, columnspan=3, **pad)
        self.bar = ttk.Progressbar(frame, mode="determinate", maximum=1.0)
        self.bar.grid(row=6, column=0, columnspan=3, sticky="ew", **pad)
        self.log = tk.Text(frame, height=10, wrap="word", state="disabled")
        self.log.grid(row=7, column=0, columnspan=3, sticky="nsew", **pad)
        frame.rowconfigure(7, weight=1)

        self.output.trace_add("write", lambda *args: self.update_button())
        self.mode_changed()
        self.after(100, self.poll)

    # ---- choices ----

    def pick_exe(self):
        path = filedialog.askopenfilename(title="CMR2.exe of your PC installation",
                                          filetypes=[("CMR2.exe", "*.exe *.EXE"), ("All files", "*")])
        if path:
            self.exe.set(path)
            self.check()

    def pick_output(self):
        if self.mode.get() == "cia":
            path = filedialog.asksaveasfilename(title="Save the CIA as", defaultextension=".cia",
                                                initialfile="OpenCMR2-full.cia", filetypes=[("CIA", "*.cia")])
        else:
            path = filedialog.askdirectory(title="The SD card (its root)")
        if path:
            self.output.set(path)
        self.update_button()

    def pick_kit(self):
        path = filedialog.askdirectory(title="The kit folder (opencmr2.elf, makerom, ...)")
        if path:
            self.kit.set(path)
        self.update_button()

    def mode_changed(self):
        cia = self.mode.get() == "cia"
        self.output_label.config(text="CIA file:" if cia else "SD card:")
        state = "normal" if cia else "disabled"
        for widget in (self.kit_entry, self.kit_button):
            widget.config(state=state)
        if cia and not self.output.get().lower().endswith(".cia"):
            self.output.set(str(Path.home() / "OpenCMR2-full.cia"))
        elif not cia and self.output.get().lower().endswith(".cia"):
            self.output.set("")
        self.update_button()

    def check(self):
        try:
            self.install = builder.inspect(Path(self.exe.get()))
        except OSError as error:
            self.install = None
            self.status.config(text=f"Cannot read it: {error}")
        else:
            self.status.config(text=builder.describe(self.install))
        self.update_button()

    def update_button(self):
        ready = (not self.busy and self.install is not None and self.install.ok and bool(self.output.get()))
        self.build_button.config(state="normal" if ready else "disabled")

    # ---- work ----

    def build(self):
        if self.mode.get() == "cia":
            missing = builder.check_kit(Path(self.kit.get()))
            if missing:
                messagebox.showerror("Kit incomplete", "The kit folder lacks:\n" + "\n".join(missing))
                return
        self.busy = True
        self.update_button()
        self.write(f"--- {('CIA' if self.mode.get() == 'cia' else 'SD card')}: {self.output.get()}")
        threading.Thread(target=self.work, daemon=True).start()

    def work(self):
        def progress(message, fraction):
            self.events.put(("progress", message, fraction))
        try:
            if self.mode.get() == "cia":
                builder.build_cia(self.install, Path(self.kit.get()), Path(self.output.get()), progress)
            else:
                builder.build_sd(self.install, Path(self.output.get()), progress)
            self.events.put(("done", None, None))
        except Exception as error:   # shown to the user, never lost in a thread
            details = traceback.format_exc()
            print(details, file=sys.stderr, flush=True)   # the debug .bat's console
            self.events.put(("error", str(error), details))

    def poll(self):
        try:
            while True:
                kind, message, fraction = self.events.get_nowait()
                if kind == "progress":
                    if fraction < 0:
                        self.bar.config(mode="indeterminate")
                        self.bar.start(15)
                    else:
                        self.bar.stop()
                        self.bar.config(mode="determinate", value=fraction)
                    if fraction < 0 or fraction >= 1 or not message.startswith(("Copying", "Preparing")):
                        self.write(message)
                    else:
                        self.status.config(text=message)
                else:
                    self.bar.stop()
                    self.bar.config(mode="determinate", value=1.0 if kind == "done" else 0.0)
                    self.busy = False
                    self.update_button()
                    if self.install:
                        self.status.config(text=builder.describe(self.install))
                    if kind == "error":
                        self.write("Error: " + message)
                        self.write(fraction)   # the traceback, to copy from the log
                        messagebox.showerror("OpenCMR2 CIA Builder", message)
                    else:
                        self.write("Done.")
        except queue.Empty:
            pass
        self.after(100, self.poll)

    def write(self, line):
        self.log.config(state="normal")
        self.log.insert("end", line + "\n")
        self.log.see("end")
        self.log.config(state="disabled")


def main():
    try:
        App().mainloop()
    except Exception:
        # Started without a console (pyw), a crash would leave no trace.
        details = traceback.format_exc()
        print(details, file=sys.stderr, flush=True)
        log = Path(__file__).resolve().parent / "cia_builder_error.log"
        try:
            log.write_text(details, encoding="utf-8")
        except OSError:
            pass
        try:
            messagebox.showerror("OpenCMR2 CIA Builder", f"The CIA Builder stopped:\n\n{details}\nSaved to {log}")
        except Exception:
            pass
        sys.exit(1)


if __name__ == "__main__":
    main()
