import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import os


class FileSystemGUI:
    def __init__(self, root, filesystem):
        self.root = root
        self.fs = filesystem
        self.root.title("File System Virtual")
        self.root.geometry("1250x780")
        self.root.minsize(900, 600)

        self._setup_styles()
        self._create_widgets()

    # ── Estilos ───────────────────────────────────────────────────────────────

    def _setup_styles(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TButton", padding=(6, 4))
        style.configure("Nav.TButton", padding=(6, 4), foreground="#1565c0")
        style.configure("Danger.TButton", padding=(6, 4), foreground="#b71c1c")

    # ── Layout principal ──────────────────────────────────────────────────────

    def _create_widgets(self):
        paned = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Panel izquierdo: árbol de archivos
        left = ttk.LabelFrame(paned, text="Sistema de Archivos", padding=4)
        paned.add(left, weight=1)
        left.configure(width=260)
        left.pack_propagate(False)

        tree_scroll = ttk.Scrollbar(left)
        tree_scroll.pack(side=tk.RIGHT, fill=tk.Y)

        self.tree = ttk.Treeview(left, yscrollcommand=tree_scroll.set, selectmode="browse")
        tree_scroll.config(command=self.tree.yview)
        self.tree.pack(fill=tk.BOTH, expand=True)
        self.tree.bind("<Double-1>", self._on_tree_double_click)

        # Etiqueta de leyenda en el árbol
        ttk.Label(left, text="Doble clic en carpeta para navegar",
                  foreground="gray", font=("TkDefaultFont", 8)).pack(pady=2)

        # Panel derecho: info + comandos + salida
        right = ttk.Frame(paned)
        paned.add(right, weight=3)

        # -- Barra de información actual --
        info_frame = ttk.LabelFrame(right, text="Informacion Actual", padding=6)
        info_frame.pack(fill=tk.X, padx=4, pady=(4, 2))

        self.path_label = ttk.Label(info_frame, text="Ruta: /",
                                    font=("Consolas", 10, "bold"), foreground="#1565c0")
        self.path_label.pack(fill=tk.X, anchor=tk.W)

        disk_row = ttk.Frame(info_frame)
        disk_row.pack(fill=tk.X, pady=(4, 0))
        self.disk_label = ttk.Label(disk_row, text="Disco: sin disco creado", foreground="gray")
        self.disk_label.pack(side=tk.LEFT)

        self.disk_bar = ttk.Progressbar(info_frame, maximum=100, value=0)
        self.disk_bar.pack(fill=tk.X, pady=(3, 0))

        # -- Comandos --
        cmd_frame = ttk.LabelFrame(right, text="Comandos", padding=6)
        cmd_frame.pack(fill=tk.X, padx=4, pady=2)

        # Fila 1: Navegación y disco
        r1 = ttk.Frame(cmd_frame)
        r1.pack(fill=tk.X, pady=2)
        ttk.Label(r1, text="Navegar:").pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(r1, text="Subir [..]", style="Nav.TButton",
                   command=self._cmd_go_up).pack(side=tk.LEFT, padx=2)
        ttk.Button(r1, text="CambiarDIR", style="Nav.TButton",
                   command=self._cmd_cd).pack(side=tk.LEFT, padx=2)
        ttk.Button(r1, text="ListarDIR", command=self._cmd_listar).pack(side=tk.LEFT, padx=2)
        ttk.Button(r1, text="TREE", command=self._cmd_tree).pack(side=tk.LEFT, padx=2)
        ttk.Separator(r1, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)
        ttk.Label(r1, text="Disco:").pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(r1, text="CREATE", command=self._cmd_create).pack(side=tk.LEFT, padx=2)

        # Fila 2: Operaciones de archivos y directorios
        r2 = ttk.Frame(cmd_frame)
        r2.pack(fill=tk.X, pady=2)
        ttk.Label(r2, text="Crear:").pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(r2, text="MKDIR", command=self._cmd_mkdir).pack(side=tk.LEFT, padx=2)
        ttk.Button(r2, text="FILE", command=self._cmd_file).pack(side=tk.LEFT, padx=2)
        ttk.Separator(r2, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)
        ttk.Label(r2, text="Archivo:").pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(r2, text="VerFile", command=self._cmd_ver_file).pack(side=tk.LEFT, padx=2)
        ttk.Button(r2, text="VerPropiedades", command=self._cmd_propiedades).pack(side=tk.LEFT, padx=2)
        ttk.Button(r2, text="ModFILE", command=self._cmd_mod_file).pack(side=tk.LEFT, padx=2)

        # Fila 3: Operaciones avanzadas
        r3 = ttk.Frame(cmd_frame)
        r3.pack(fill=tk.X, pady=2)
        ttk.Label(r3, text="Gestión:").pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(r3, text="CoPY", command=self._cmd_copy).pack(side=tk.LEFT, padx=2)
        ttk.Button(r3, text="MoVer", command=self._cmd_mover).pack(side=tk.LEFT, padx=2)
        ttk.Button(r3, text="ReMove", style="Danger.TButton",
                   command=self._cmd_remove).pack(side=tk.LEFT, padx=2)
        ttk.Button(r3, text="FIND", command=self._cmd_find).pack(side=tk.LEFT, padx=2)
        ttk.Separator(r3, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)
        ttk.Button(r3, text="Limpiar salida", command=self._cmd_clear).pack(side=tk.LEFT, padx=2)

        # -- Área de salida --
        out_frame = ttk.LabelFrame(right, text="Salida", padding=6)
        out_frame.pack(fill=tk.BOTH, expand=True, padx=4, pady=(2, 4))

        out_scroll = ttk.Scrollbar(out_frame)
        out_scroll.pack(side=tk.RIGHT, fill=tk.Y)

        self.output = tk.Text(
            out_frame, state=tk.DISABLED, wrap=tk.WORD,
            yscrollcommand=out_scroll.set,
            bg="#1e1e2e", fg="#cdd6f4",
            font=("Consolas", 10), relief=tk.FLAT,
            insertbackground="white",
        )
        out_scroll.config(command=self.output.yview)
        self.output.pack(fill=tk.BOTH, expand=True)

        # Colores para distintos tipos de mensaje
        self.output.tag_configure("ok", foreground="#a6e3a1")
        self.output.tag_configure("err", foreground="#f38ba8")
        self.output.tag_configure("info", foreground="#89b4fa")
        self.output.tag_configure("dim", foreground="#6c7086")

        # -- Barra de estado --
        status_bar = ttk.Frame(self.root, relief=tk.SUNKEN, padding=(4, 2))
        status_bar.pack(fill=tk.X, side=tk.BOTTOM)
        self.status_label = ttk.Label(status_bar, text="Listo", anchor=tk.W)
        self.status_label.pack(fill=tk.X)

        # Render inicial
        self._update_info()

    # ── Árbol de archivos ─────────────────────────────────────────────────────

    def _refresh_tree(self):
        for item in self.tree.get_children():
            self.tree.delete(item)
        if self.fs.root:
            self._add_tree_node(self.fs.root, "")
        items = self.tree.get_children()
        if items:
            self.tree.item(items[0], open=True)

    def _add_tree_node(self, directory, parent):
        path = self._dir_path(directory)
        current_path = self._current_path()
        tag = ("current",) if path == current_path else ()
        node_id = self.tree.insert(
            parent, "end",
            text=f"[+] {directory.name}",
            values=(path, "dir"),
            tags=tag,
            open=(path == "/" or path == current_path),
        )
        self.tree.tag_configure("current", foreground="#1565c0", font=("TkDefaultFont", 9, "bold"))

        for subdir in [v for _, v in sorted(directory.subdirs.items())]:
            self._add_tree_node(subdir, node_id)
        for fname, fobj in sorted(directory.files.items()):
            self.tree.insert(
                node_id, "end",
                text=f"    {fname}  ({fobj.size}B)",
                values=(path + "/" + fname, "file"),
            )

    def _on_tree_double_click(self, _event):
        item = self.tree.selection()
        if not item:
            return
        values = self.tree.item(item[0])["values"]
        if not values or len(values) < 2:
            return
        path, kind = values[0], values[1]
        if kind == "dir":
            result = self.fs.CambiarDIR(path)
            self._print(result, "info")
            self._update_info()

    # ── Actualizar UI ─────────────────────────────────────────────────────────

    def _update_info(self):
        path = self._current_path()
        self.path_label.config(text=f"Ruta actual: {path}")

        if self.fs.disk:
            st = self.fs.disk.get_disk_status()
            pct = st["percentage"]
            color = "#b71c1c" if pct > 85 else "#e65100" if pct > 60 else "#1b5e20"
            self.disk_label.config(
                text=(f"Disco: {st['used']}/{st['total']} sectores  |  "
                      f"{st['free']} libres  |  {pct:.1f}% ocupado"),
                foreground=color,
            )
            self.disk_bar.config(value=pct)
        else:
            self.disk_label.config(text="Disco: sin disco creado", foreground="gray")
            self.disk_bar.config(value=0)

        self._refresh_tree()
        self.status_label.config(text=f"Directorio: {path}")

    def _print(self, message, tag="ok"):
        self.output.config(state=tk.NORMAL)
        if tag == "err" or (isinstance(message, str) and message.startswith("Error")):
            tag = "err"
        self.output.insert(tk.END, str(message) + "\n", tag)
        self.output.see(tk.END)
        self.output.config(state=tk.DISABLED)

    def _current_path(self):
        path = []
        current = self.fs.current_dir
        while current.parent is not None:
            path.insert(0, current.name)
            current = current.parent
        return "/" + "/".join(path) if path else "/"

    def _dir_path(self, directory):
        path = []
        current = directory
        while current.parent is not None:
            path.insert(0, current.name)
            current = current.parent
        return "/" + "/".join(path) if path else "/"

    def _ask_overwrite(self, what):
        return messagebox.askyesno("Confirmar", f"'{what}' ya existe.\n¿Desea reemplazarlo?",
                                   parent=self.root)

    # ── Handlers de comandos ──────────────────────────────────────────────────

    def _cmd_create(self):
        dialog = CreateDiskDialog(self.root)
        if not dialog.result:
            return
        sectors, size = dialog.result
        try:
            result = self.fs.CREATE(sectors, size)
            self._print(result)
        except Exception as e:
            if "DISK_EXISTS" in str(e):
                if self._ask_overwrite("El disco"):
                    result = self.fs.CREATE(sectors, size, overwrite=True)
                    self._print(result)
            else:
                self._print(f"Error: {e}", "err")
        self._update_info()

    def _cmd_mkdir(self):
        dialog = SimpleInputDialog(self.root, "Crear Directorio", "Nombre del directorio:")
        if not dialog.result:
            return
        name = dialog.result.strip()
        if not name:
            return
        try:
            result = self.fs.MKDIR(name)
            self._print(result)
        except Exception as e:
            if "DIR_EXISTS" in str(e):
                if self._ask_overwrite(name):
                    result = self.fs.MKDIR(name, overwrite=True)
                    self._print(result)
            else:
                self._print(f"Error: {e}", "err")
        self._update_info()

    def _cmd_file(self):
        if not self.fs.disk:
            messagebox.showwarning("Advertencia", "Debe crear un disco primero (CREATE).",
                                   parent=self.root)
            return
        dialog = CreateFileDialog(self.root)
        if not dialog.result:
            return
        name, ext, content = dialog.result
        try:
            result = self.fs.FILE(name, ext, content)
            self._print(result)
        except Exception as e:
            if "FILE_EXISTS" in str(e):
                if self._ask_overwrite(f"{name}.{ext}"):
                    result = self.fs.FILE(name, ext, content, overwrite=True)
                    self._print(result)
            else:
                self._print(f"Error: {e}", "err")
        self._update_info()

    def _cmd_cd(self):
        dialog = CambiarDirDialog(self.root, self.fs)
        if not dialog.result:
            return
        result = self.fs.CambiarDIR(dialog.result)
        self._print(result, "info")
        self._update_info()

    def _cmd_go_up(self):
        if self.fs.current_dir.parent is None:
            self._print("Ya estas en la raiz.", "dim")
            return
        result = self.fs.CambiarDIR("..")
        self._print(result, "info")
        self._update_info()

    def _cmd_listar(self):
        result = self.fs.ListarDIR()
        self._print(result, "info")

    def _cmd_ver_file(self):
        files = self.fs.current_dir.files
        if not files:
            messagebox.showwarning("Advertencia", "No hay archivos en el directorio actual.",
                                   parent=self.root)
            return
        items = sorted(files.keys())
        dialog = SelectDialog(self.root, "Ver Archivo", items)
        if dialog.result:
            result = self.fs.VerFile(dialog.result)
            self._print(result, "info")

    def _cmd_propiedades(self):
        files = self.fs.current_dir.files
        if not files:
            messagebox.showwarning("Advertencia", "No hay archivos en el directorio actual.",
                                   parent=self.root)
            return
        items = sorted(files.keys())
        dialog = SelectDialog(self.root, "Propiedades de Archivo", items)
        if dialog.result:
            result = self.fs.VerPropiedades(dialog.result)
            self._print(result, "info")

    def _cmd_mod_file(self):
        files = self.fs.current_dir.files
        if not files:
            messagebox.showwarning("Advertencia", "No hay archivos en el directorio actual.",
                                   parent=self.root)
            return
        items = sorted(files.keys())
        dialog = SelectDialog(self.root, "Seleccionar Archivo a Modificar", items)
        if not dialog.result:
            return
        fullname = dialog.result
        current_content = files[fullname].content or ""
        mod_dialog = ModifyFileDialog(self.root, fullname, current_content)
        if mod_dialog.result is not None:
            result = self.fs.ModFILE(fullname, mod_dialog.result)
            self._print(result)
            self._update_info()

    def _cmd_copy(self):
        if not self.fs.disk:
            messagebox.showwarning("Advertencia", "Debe crear un disco primero (CREATE).",
                                   parent=self.root)
            return
        dialog = CopyDialog(self.root, self.fs)
        if not dialog.result:
            return
        source, dest, copy_type = dialog.result
        try:
            result = self.fs.COPY(source, dest, copy_type)
            self._print(result)
        except Exception as e:
            if "FILE_EXISTS" in str(e):
                if self._ask_overwrite(os.path.basename(source)):
                    result = self.fs.COPY(source, dest, copy_type, overwrite=True)
                    self._print(result)
            else:
                self._print(f"Error: {e}", "err")
        self._update_info()

    def _cmd_mover(self):
        files = list(self.fs.current_dir.files.keys())
        dirs = list(self.fs.current_dir.subdirs.keys())
        items = sorted(files) + sorted(dirs)
        if not items:
            messagebox.showwarning("Advertencia", "No hay elementos en el directorio actual.",
                                   parent=self.root)
            return
        dialog = MoverDialog(self.root, items)
        if dialog.result:
            source, dest = dialog.result
            result = self.fs.MoVer(source, dest)
            self._print(result)
            self._update_info()

    def _cmd_remove(self):
        selected = self.tree.selection()
        if selected:
            values = self.tree.item(selected[0])["values"]
            if values and len(values) >= 2:
                path, kind = values[0], values[1]
                if kind == "dir":
                    self.fs.CambiarDIR(path)
                    self._update_info()
                elif kind == "file":
                    parent_path = path.rsplit("/", 1)[0] or "/"
                    self.fs.CambiarDIR(parent_path)
                    self._update_info()

        files = list(self.fs.current_dir.files.keys())
        dirs = list(self.fs.current_dir.subdirs.keys())
        items = sorted(files) + sorted(dirs)
        if not items:
            messagebox.showwarning("Advertencia", "No hay elementos en el directorio actual.",
                                   parent=self.root)
            return
        dialog = SelectDialog(self.root, "Eliminar", items)
        if not dialog.result:
            return
        target = dialog.result
        recursive = False
        if target in dirs:
            subdir = self.fs.current_dir.subdirs[target]
            if subdir.subdirs or subdir.files:
                recursive = messagebox.askyesno(
                    "Directorio no vacio",
                    f"'{target}' contiene elementos.\n¿Eliminar recursivamente?",
                    parent=self.root,
                )
        if messagebox.askyesno("Confirmar", f"¿Eliminar '{target}'?", parent=self.root):
            result = self.fs.ReMove(target, recursive)
            self._print(result, "err")
            self._update_info()

    def _cmd_find(self):
        dialog = SimpleInputDialog(self.root, "Buscar", "Patron (ej: *.txt, nombre, doc*):")
        if dialog.result:
            result = self.fs.FIND(dialog.result.strip())
            self._print(result, "info")

    def _cmd_tree(self):
        result = self.fs.TREE()
        self._print("\n" + result + "\n", "dim")

    def _cmd_clear(self):
        self.output.config(state=tk.NORMAL)
        self.output.delete(1.0, tk.END)
        self.output.config(state=tk.DISABLED)


# ── Diálogos ─────────────────────────────────────────────────────────────────

class CreateDiskDialog(tk.Toplevel):
    def __init__(self, parent):
        super().__init__(parent)
        self.title("Crear Disco Virtual")
        self.geometry("340x200")
        self.resizable(False, False)
        self.result = None

        ttk.Label(self, text="Crear Disco Virtual", font=("TkDefaultFont", 11, "bold")).grid(
            row=0, column=0, columnspan=2, pady=(15, 10))

        ttk.Label(self, text="Numero de sectores:").grid(row=1, column=0, padx=20, pady=8, sticky=tk.W)
        self.sectors_var = tk.StringVar(value="20")
        ttk.Entry(self, textvariable=self.sectors_var, width=12).grid(row=1, column=1, padx=10, pady=8)

        ttk.Label(self, text="Tamano de sector (bytes):").grid(row=2, column=0, padx=20, pady=8, sticky=tk.W)
        self.size_var = tk.StringVar(value="64")
        ttk.Entry(self, textvariable=self.size_var, width=12).grid(row=2, column=1, padx=10, pady=8)

        self._preview = ttk.Label(self, text="Total: 1280 bytes", foreground="gray")
        self._preview.grid(row=3, column=0, columnspan=2, pady=2)
        self.sectors_var.trace_add("write", self._update_preview)
        self.size_var.trace_add("write", self._update_preview)

        btn_frame = ttk.Frame(self)
        btn_frame.grid(row=4, column=0, columnspan=2, pady=15)
        ttk.Button(btn_frame, text="Crear", command=self._ok).pack(side=tk.LEFT, padx=6)
        ttk.Button(btn_frame, text="Cancelar", command=self.destroy).pack(side=tk.LEFT, padx=6)

        self.transient(parent)
        self.grab_set()
        parent.wait_window(self)

    def _update_preview(self, *_):
        try:
            total = int(self.sectors_var.get()) * int(self.size_var.get())
            self._preview.config(text=f"Total: {total} bytes")
        except ValueError:
            self._preview.config(text="")

    def _ok(self):
        try:
            sectors = int(self.sectors_var.get())
            size = int(self.size_var.get())
            if sectors <= 0 or size <= 0:
                messagebox.showerror("Error", "Los valores deben ser positivos.", parent=self)
                return
            self.result = (sectors, size)
            self.destroy()
        except ValueError:
            messagebox.showerror("Error", "Ingresa numeros validos.", parent=self)


class CreateFileDialog(tk.Toplevel):
    def __init__(self, parent):
        super().__init__(parent)
        self.title("Crear Archivo")
        self.geometry("460x360")
        self.resizable(True, True)
        self.result = None

        ttk.Label(self, text="Nombre:").grid(row=0, column=0, padx=15, pady=10, sticky=tk.W)
        self.name_var = tk.StringVar()
        ttk.Entry(self, textvariable=self.name_var, width=30).grid(row=0, column=1, padx=10, pady=10, sticky=tk.EW)

        ttk.Label(self, text="Extension:").grid(row=1, column=0, padx=15, pady=5, sticky=tk.W)
        self.ext_var = tk.StringVar(value="txt")
        ttk.Entry(self, textvariable=self.ext_var, width=12).grid(row=1, column=1, padx=10, pady=5, sticky=tk.W)

        ttk.Label(self, text="Contenido:").grid(row=2, column=0, padx=15, pady=5, sticky=tk.NW)
        frame = ttk.Frame(self)
        frame.grid(row=2, column=1, padx=10, pady=5, sticky=tk.NSEW)
        scroll = ttk.Scrollbar(frame)
        scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.content_text = tk.Text(frame, height=12, width=36, yscrollcommand=scroll.set)
        scroll.config(command=self.content_text.yview)
        self.content_text.pack(fill=tk.BOTH, expand=True)

        self.columnconfigure(1, weight=1)
        self.rowconfigure(2, weight=1)

        btn_frame = ttk.Frame(self)
        btn_frame.grid(row=3, column=0, columnspan=2, pady=12)
        ttk.Button(btn_frame, text="Crear", command=self._ok).pack(side=tk.LEFT, padx=6)
        ttk.Button(btn_frame, text="Cancelar", command=self.destroy).pack(side=tk.LEFT, padx=6)

        self.transient(parent)
        self.grab_set()
        parent.wait_window(self)

    def _ok(self):
        name = self.name_var.get().strip()
        ext = self.ext_var.get().strip() or "txt"
        content = self.content_text.get(1.0, tk.END).rstrip("\n")
        if not name:
            messagebox.showerror("Error", "Ingresa un nombre para el archivo.", parent=self)
            return
        self.result = (name, ext, content)
        self.destroy()


class SimpleInputDialog(tk.Toplevel):
    def __init__(self, parent, title, prompt):
        super().__init__(parent)
        self.title(title)
        self.geometry("360x130")
        self.resizable(False, False)
        self.result = None

        ttk.Label(self, text=prompt).pack(padx=20, pady=(15, 5))
        self.entry_var = tk.StringVar()
        entry = ttk.Entry(self, textvariable=self.entry_var, width=38)
        entry.pack(padx=20, pady=5)
        entry.focus_set()
        self.bind("<Return>", lambda _: self._ok())

        btn_frame = ttk.Frame(self)
        btn_frame.pack(pady=12)
        ttk.Button(btn_frame, text="OK", command=self._ok).pack(side=tk.LEFT, padx=6)
        ttk.Button(btn_frame, text="Cancelar", command=self.destroy).pack(side=tk.LEFT, padx=6)

        self.transient(parent)
        self.grab_set()
        parent.wait_window(self)

    def _ok(self):
        val = self.entry_var.get().strip()
        if val:
            self.result = val
        self.destroy()


class SelectDialog(tk.Toplevel):
    def __init__(self, parent, title, items):
        super().__init__(parent)
        self.title(title)
        self.geometry("320x260")
        self.result = None

        ttk.Label(self, text="Selecciona un elemento:").pack(padx=12, pady=(12, 4))

        frame = ttk.Frame(self)
        frame.pack(fill=tk.BOTH, expand=True, padx=12, pady=4)
        scroll = ttk.Scrollbar(frame)
        scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.listbox = tk.Listbox(frame, yscrollcommand=scroll.set, height=10)
        scroll.config(command=self.listbox.yview)
        self.listbox.pack(fill=tk.BOTH, expand=True)

        for item in items:
            self.listbox.insert(tk.END, item)

        self.listbox.bind("<Double-1>", lambda _: self._ok())

        btn_frame = ttk.Frame(self)
        btn_frame.pack(pady=10)
        ttk.Button(btn_frame, text="Seleccionar", command=self._ok).pack(side=tk.LEFT, padx=6)
        ttk.Button(btn_frame, text="Cancelar", command=self.destroy).pack(side=tk.LEFT, padx=6)

        self.transient(parent)
        self.grab_set()
        parent.wait_window(self)

    def _ok(self):
        sel = self.listbox.curselection()
        if sel:
            self.result = self.listbox.get(sel[0])
        self.destroy()


class ModifyFileDialog(tk.Toplevel):
    def __init__(self, parent, filename, current_content):
        super().__init__(parent)
        self.title(f"Modificar: {filename}")
        self.geometry("540x420")
        self.resizable(True, True)
        self.result = None

        ttk.Label(self, text=f"Contenido de '{filename}':").pack(padx=12, pady=(12, 4), anchor=tk.W)

        frame = ttk.Frame(self)
        frame.pack(fill=tk.BOTH, expand=True, padx=12, pady=4)
        scroll = ttk.Scrollbar(frame)
        scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.text = tk.Text(frame, yscrollcommand=scroll.set, wrap=tk.WORD,
                            font=("Consolas", 10))
        scroll.config(command=self.text.yview)
        self.text.pack(fill=tk.BOTH, expand=True)
        self.text.insert(1.0, current_content)

        btn_frame = ttk.Frame(self)
        btn_frame.pack(pady=10)
        ttk.Button(btn_frame, text="Guardar", command=self._ok).pack(side=tk.LEFT, padx=6)
        ttk.Button(btn_frame, text="Cancelar", command=self.destroy).pack(side=tk.LEFT, padx=6)

        self.transient(parent)
        self.grab_set()
        parent.wait_window(self)

    def _ok(self):
        self.result = self.text.get(1.0, tk.END).rstrip("\n")
        self.destroy()


class CambiarDirDialog(tk.Toplevel):
    def __init__(self, parent, fs):
        super().__init__(parent)
        self.title("Cambiar Directorio (CD)")
        self.geometry("420x300")
        self.result = None
        self.fs = fs

        ttk.Label(self, text="Ruta destino (absoluta o relativa):").pack(
            padx=15, pady=(15, 4), anchor=tk.W)

        self.path_var = tk.StringVar(value="/")
        entry = ttk.Entry(self, textvariable=self.path_var, width=48)
        entry.pack(padx=15, pady=4, fill=tk.X)
        entry.focus_set()
        self.bind("<Return>", lambda _: self._ok())

        ttk.Label(self, text="Accesos rapidos:", foreground="gray").pack(
            padx=15, pady=(10, 2), anchor=tk.W)

        frame = ttk.Frame(self)
        frame.pack(fill=tk.BOTH, expand=True, padx=15, pady=4)
        scroll = ttk.Scrollbar(frame)
        scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.listbox = tk.Listbox(frame, yscrollcommand=scroll.set, height=7)
        scroll.config(command=self.listbox.yview)
        self.listbox.pack(fill=tk.BOTH, expand=True)

        if fs.current_dir.parent is not None:
            self.listbox.insert(tk.END, "..  (directorio padre)")
        self.listbox.insert(tk.END, "/  (raiz)")
        for d in sorted(fs.current_dir.subdirs.keys()):
            self.listbox.insert(tk.END, d)

        self.listbox.bind("<Double-1>", self._on_listbox_pick)

        btn_frame = ttk.Frame(self)
        btn_frame.pack(pady=10)
        ttk.Button(btn_frame, text="Ir", command=self._ok).pack(side=tk.LEFT, padx=6)
        ttk.Button(btn_frame, text="Cancelar", command=self.destroy).pack(side=tk.LEFT, padx=6)

        self.transient(parent)
        self.grab_set()
        parent.wait_window(self)

    def _on_listbox_pick(self, _event):
        sel = self.listbox.curselection()
        if sel:
            raw = self.listbox.get(sel[0]).split()[0]
            self.path_var.set(raw)

    def _ok(self):
        val = self.path_var.get().strip()
        if val:
            self.result = val
        self.destroy()


class MoverDialog(tk.Toplevel):
    def __init__(self, parent, items):
        super().__init__(parent)
        self.title("Mover / Renombrar")
        self.geometry("380x300")
        self.resizable(False, True)
        self.result = None

        ttk.Label(self, text="Selecciona el elemento a mover/renombrar:").pack(
            padx=15, pady=(15, 4), anchor=tk.W)

        frame = ttk.Frame(self)
        frame.pack(fill=tk.BOTH, expand=True, padx=15, pady=4)
        scroll = ttk.Scrollbar(frame)
        scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.listbox = tk.Listbox(frame, yscrollcommand=scroll.set, height=7)
        scroll.config(command=self.listbox.yview)
        self.listbox.pack(fill=tk.BOTH, expand=True)
        for item in items:
            self.listbox.insert(tk.END, item)

        ttk.Label(self, text="Nuevo nombre o ruta destino (ej: nuevo, /dir/nuevo):").pack(
            padx=15, pady=(8, 2), anchor=tk.W)
        self.dest_var = tk.StringVar()
        ttk.Entry(self, textvariable=self.dest_var, width=40).pack(padx=15, pady=4, fill=tk.X)

        btn_frame = ttk.Frame(self)
        btn_frame.pack(pady=10)
        ttk.Button(btn_frame, text="Mover", command=self._ok).pack(side=tk.LEFT, padx=6)
        ttk.Button(btn_frame, text="Cancelar", command=self.destroy).pack(side=tk.LEFT, padx=6)

        self.transient(parent)
        self.grab_set()
        parent.wait_window(self)

    def _ok(self):
        sel = self.listbox.curselection()
        dest = self.dest_var.get().strip()
        if not sel:
            messagebox.showerror("Error", "Selecciona un elemento.", parent=self)
            return
        if not dest:
            messagebox.showerror("Error", "Ingresa el nombre o ruta destino.", parent=self)
            return
        self.result = (self.listbox.get(sel[0]), dest)
        self.destroy()


class CopyDialog(tk.Toplevel):
    def __init__(self, parent, fs):
        super().__init__(parent)
        self.title("Copiar Archivo / Directorio")
        self.geometry("480x320")
        self.resizable(False, False)
        self.result = None
        self.fs = fs

        ttk.Label(self, text="Tipo de copia:").pack(padx=15, pady=(15, 4), anchor=tk.W)

        self.copy_type = tk.StringVar(value="virtual_to_virtual")
        rb_frame = ttk.Frame(self)
        rb_frame.pack(fill=tk.X, padx=25)
        ttk.Radiobutton(rb_frame, text="Virtual -> Virtual  (dentro del FS)",
                        variable=self.copy_type, value="virtual_to_virtual").pack(anchor=tk.W)
        ttk.Radiobutton(rb_frame, text="Real -> Virtual  (importar desde PC)",
                        variable=self.copy_type, value="real_to_virtual").pack(anchor=tk.W)
        ttk.Radiobutton(rb_frame, text="Virtual -> Real  (exportar a PC)",
                        variable=self.copy_type, value="virtual_to_real").pack(anchor=tk.W)

        # Origen
        src_frame = ttk.Frame(self)
        src_frame.pack(fill=tk.X, padx=15, pady=(12, 4))
        ttk.Label(src_frame, text="Origen:", width=8).pack(side=tk.LEFT)
        self.source_var = tk.StringVar()
        ttk.Entry(src_frame, textvariable=self.source_var).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4)
        ttk.Button(src_frame, text="...", width=3, command=self._browse_src).pack(side=tk.LEFT)

        # Destino
        dst_frame = ttk.Frame(self)
        dst_frame.pack(fill=tk.X, padx=15, pady=4)
        ttk.Label(dst_frame, text="Destino:", width=8).pack(side=tk.LEFT)
        self.dest_var = tk.StringVar()
        ttk.Entry(dst_frame, textvariable=self.dest_var).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4)
        ttk.Button(dst_frame, text="...", width=3, command=self._browse_dst).pack(side=tk.LEFT)

        hint = ttk.Label(self,
                         text="Virtual->Virtual: destino = nuevo_nombre.ext  o  nombre_carpeta\n"
                              "Real->Virtual: origen = ruta PC (archivo o carpeta)\n"
                              "Virtual->Real: origen = nombre.ext  o  nombre_carpeta",
                         foreground="gray", font=("TkDefaultFont", 8))
        hint.pack(padx=15, pady=4, anchor=tk.W)

        btn_frame = ttk.Frame(self)
        btn_frame.pack(pady=10)
        ttk.Button(btn_frame, text="Copiar", command=self._ok).pack(side=tk.LEFT, padx=6)
        ttk.Button(btn_frame, text="Cancelar", command=self.destroy).pack(side=tk.LEFT, padx=6)

        self.transient(parent)
        self.grab_set()
        parent.wait_window(self)

    def _browse_src(self):
        ctype = self.copy_type.get()
        if ctype == "real_to_virtual":
            choice = messagebox.askyesno(
                "Tipo de origen", "¿Copiar un archivo?\n(No = Copiar una carpeta)", parent=self)
            if choice:
                path = filedialog.askopenfilename(parent=self, title="Selecciona archivo real")
            else:
                path = filedialog.askdirectory(parent=self, title="Selecciona carpeta real")
            if path:
                self.source_var.set(path)
        else:
            files = sorted(self.fs.current_dir.files.keys())
            dirs = sorted(self.fs.current_dir.subdirs.keys())
            items = files + dirs
            if items:
                dlg = SelectDialog(self, "Elemento virtual origen", items)
                if dlg.result:
                    self.source_var.set(dlg.result)

    def _browse_dst(self):
        ctype = self.copy_type.get()
        if ctype == "virtual_to_real":
            src = self.source_var.get().strip()
            if src in self.fs.current_dir.subdirs:
                path = filedialog.askdirectory(parent=self, title="Selecciona carpeta destino")
            else:
                path = filedialog.asksaveasfilename(parent=self, title="Guardar como")
            if path:
                self.dest_var.set(path)

    def _ok(self):
        source = self.source_var.get().strip()
        dest = self.dest_var.get().strip()
        if not source or not dest:
            messagebox.showerror("Error", "Completa los campos de origen y destino.", parent=self)
            return
        self.result = (source, dest, self.copy_type.get())
        self.destroy()
