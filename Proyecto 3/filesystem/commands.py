import os
import fnmatch
from filesystem.directory import Directory
from filesystem.file import File
from filesystem.disk import VirtualDisk


class FileSystem:
    def __init__(self):
        self.disk = None
        self.root = Directory("root")
        self.current_dir = self.root

    def _get_current_path(self):
        path = []
        current = self.current_dir
        while current.parent is not None:
            path.insert(0, current.name)
            current = current.parent
        return "/" + "/".join(path) if path else "/"

    def _find_directory(self, path):
        if path == "/" or path == "":
            return self.root

        if path.startswith("/"):
            current = self.root
            path = path[1:]
        else:
            current = self.current_dir

        if not path:
            return current

        for part in path.split("/"):
            if part == "..":
                if current.parent:
                    current = current.parent
            elif part and part != ".":
                if part in current.subdirs:
                    current = current.subdirs[part]
                else:
                    return None
        return current

    # ── Comandos principales ──────────────────────────────────────────────────

    def CREATE(self, num_sectors, sector_size, overwrite=False):
        if self.disk is not None and not overwrite:
            raise Exception("DISK_EXISTS")
        self.disk = VirtualDisk(num_sectors, sector_size)
        self.root = Directory("root")
        self.current_dir = self.root
        total = num_sectors * sector_size
        return (f"Disco creado: {num_sectors} sectores x {sector_size} bytes "
                f"= {total} bytes totales.")

    def MKDIR(self, dirname, overwrite=False):
        if dirname in self.current_dir.subdirs:
            if not overwrite:
                raise Exception("DIR_EXISTS")
            self._remove_directory_recursive(self.current_dir.subdirs[dirname])
            del self.current_dir.subdirs[dirname]
        new_dir = Directory(dirname, parent=self.current_dir)
        self.current_dir.subdirs[dirname] = new_dir
        return f"Directorio '{dirname}' creado en {self._get_current_path()}."

    def FILE(self, name, extension, content, overwrite=False):
        if self.disk is None:
            return "Error: Debe crear un disco primero con CREATE."

        fullname = f"{name}.{extension}"
        if fullname in self.current_dir.files:
            if not overwrite:
                raise Exception("FILE_EXISTS")
            old_file = self.current_dir.files[fullname]
            self.disk.free(old_file.sectors)

        try:
            content_bytes = content.encode("utf-8") if content else b""
            sectors_needed = max(
                1,
                (len(content_bytes) + self.disk.sector_size - 1) // self.disk.sector_size,
            )
            allocated = self.disk.allocate(sectors_needed)
            self.disk.write_data(allocated, content_bytes)
            new_file = File(name, extension, content, allocated)
            self.current_dir.files[fullname] = new_file
            return (f"Archivo '{fullname}' creado "
                    f"({sectors_needed} sector(es), {len(content_bytes)} bytes).")
        except Exception as e:
            return f"Error al crear archivo: {e}"

    def ListarDIR(self):
        path = self._get_current_path()
        lines = [f"\nContenido de '{path}':"]

        if not self.current_dir.subdirs and not self.current_dir.files:
            lines.append("  (directorio vacio)")
        else:
            for dname in sorted(self.current_dir.subdirs):
                lines.append(f"  [DIR]  {dname}/")
            for fname, fobj in sorted(self.current_dir.files.items()):
                lines.append(f"  [FILE] {fname}  ({fobj.size} bytes)")

        lines.append("")
        return "\n".join(lines)

    def CambiarDIR(self, path):
        target = self._find_directory(path)
        if target is None:
            return f"Error: El directorio '{path}' no existe."
        self.current_dir = target
        return f"Directorio actual: {self._get_current_path()}"

    def ModFILE(self, filename, new_content):
        if filename not in self.current_dir.files:
            return f"Error: El archivo '{filename}' no existe."
        if self.disk is None:
            return "Error: No hay disco creado."

        file_obj = self.current_dir.files[filename]
        old_sectors = file_obj.sectors

        try:
            content_bytes = new_content.encode("utf-8") if new_content else b""
            sectors_needed = max(
                1,
                (len(content_bytes) + self.disk.sector_size - 1) // self.disk.sector_size,
            )
            # Free old sectors before allocating new ones so they can be reused
            self.disk.free(old_sectors)
            new_sectors = self.disk.allocate(sectors_needed)
            self.disk.write_data(new_sectors, content_bytes)
            file_obj.modify(new_content)
            file_obj.sectors = new_sectors
            return f"Archivo '{filename}' modificado ({len(content_bytes)} bytes)."
        except Exception as e:
            return f"Error al modificar archivo: {e}"

    def VerPropiedades(self, filename):
        if filename not in self.current_dir.files:
            return f"Error: El archivo '{filename}' no existe."

        f = self.current_dir.files[filename]
        lines = [
            f"\nPropiedades de '{filename}':",
            f"  Nombre     : {f.name}",
            f"  Extension  : {f.extension}",
            f"  Tamano     : {f.size} bytes",
            f"  Creado     : {f.created_at}",
            f"  Modificado : {f.modified_at}",
            f"  Sectores   : {f.sectors}",
            "",
        ]
        return "\n".join(lines)

    def VerFile(self, filename):
        if filename not in self.current_dir.files:
            return f"Error: El archivo '{filename}' no existe."

        f = self.current_dir.files[filename]
        lines = [
            f"\n--- {filename} ---",
            f.content if f.content else "(vacio)",
            "--- fin ---\n",
        ]
        return "\n".join(lines)

    def COPY(self, source, destination, copy_type="virtual_to_virtual", overwrite=False):
        if copy_type == "real_to_virtual":
            if not os.path.exists(source):
                return f"Error: '{source}' no existe."
            if os.path.isdir(source):
                return self._copy_real_dir_to_virtual(source, overwrite)
            try:
                with open(source, "r", encoding="utf-8", errors="replace") as fh:
                    content = fh.read()
            except Exception as e:
                return f"Error al leer archivo: {e}"
            basename = os.path.basename(source)
            name, ext = basename.rsplit(".", 1) if "." in basename else (basename, "txt")
            return self.FILE(name, ext, content, overwrite=overwrite)

        elif copy_type == "virtual_to_real":
            if source in self.current_dir.subdirs:
                return self._copy_virtual_dir_to_real(
                    self.current_dir.subdirs[source], destination)
            if source not in self.current_dir.files:
                return f"Error: Archivo virtual '{source}' no existe."
            fobj = self.current_dir.files[source]
            dest_dir = os.path.dirname(destination)
            if dest_dir:
                os.makedirs(dest_dir, exist_ok=True)
            try:
                with open(destination, "w", encoding="utf-8") as fh:
                    fh.write(fobj.content or "")
                return f"Archivo exportado a '{destination}'."
            except Exception as e:
                return f"Error al escribir archivo: {e}"

        else:  # virtual_to_virtual
            if source in self.current_dir.subdirs:
                return self._copy_virtual_dir_to_virtual(
                    self.current_dir.subdirs[source], destination, overwrite)
            if source not in self.current_dir.files:
                return f"Error: Archivo '{source}' no existe."
            fobj = self.current_dir.files[source]
            new_name, new_ext = (
                destination.rsplit(".", 1) if "." in destination
                else (destination, fobj.extension)
            )
            return self.FILE(new_name, new_ext, fobj.content, overwrite=overwrite)

    def MoVer(self, source, destination):
        if "/" in destination:
            dest_dir_path, dest_name = destination.rsplit("/", 1)
            target_dir = self._find_directory(dest_dir_path or "/")
            if target_dir is None:
                return f"Error: Directorio destino '{dest_dir_path}' no existe."
        else:
            target_dir = self.current_dir
            dest_name = destination

        if source in self.current_dir.files:
            fobj = self.current_dir.files.pop(source)
            new_name, new_ext = (
                dest_name.rsplit(".", 1) if "." in dest_name
                else (dest_name, fobj.extension)
            )
            new_fullname = f"{new_name}.{new_ext}"
            if new_fullname in target_dir.files and self.disk:
                self.disk.free(target_dir.files[new_fullname].sectors)
            fobj.name = new_name
            fobj.extension = new_ext
            target_dir.files[new_fullname] = fobj
            return f"Archivo '{source}' movido/renombrado a '{new_fullname}'."

        elif source in self.current_dir.subdirs:
            dobj = self.current_dir.subdirs.pop(source)
            dobj.name = dest_name
            dobj.parent = target_dir
            target_dir.subdirs[dest_name] = dobj
            return f"Directorio '{source}' movido/renombrado a '{dest_name}'."

        else:
            return f"Error: '{source}' no existe en el directorio actual."

    def ReMove(self, target, recursive=False):
        if target in self.current_dir.files:
            fobj = self.current_dir.files.pop(target)
            if self.disk:
                self.disk.free(fobj.sectors)
            return f"Archivo '{target}' eliminado."

        elif target in self.current_dir.subdirs:
            subdir = self.current_dir.subdirs[target]
            if (subdir.subdirs or subdir.files) and not recursive:
                return f"El directorio '{target}' no esta vacio. Use eliminacion recursiva."
            self._remove_directory_recursive(subdir)
            del self.current_dir.subdirs[target]
            suffix = " recursivamente" if recursive else ""
            return f"Directorio '{target}' eliminado{suffix}."

        else:
            return f"Error: '{target}' no existe."

    def FIND(self, pattern):
        results = []
        self._find_recursive(self.root, pattern, results, "")

        if not results:
            return f"No se encontraron coincidencias para '{pattern}'."

        lines = [f"\nResultados para '{pattern}':"]
        for path in results:
            lines.append(f"  {path}")
        lines.append("")
        return "\n".join(lines)

    def TREE(self):
        lines = [f"{self.root.name}/"]
        self._tree_entries(self.root, "", lines)
        return "\n".join(lines)

    # ── Helpers privados ─────────────────────────────────────────────────────

    def _remove_directory_recursive(self, directory):
        for fname in list(directory.files.keys()):
            fobj = directory.files.pop(fname)
            if self.disk:
                self.disk.free(fobj.sectors)
        for dname in list(directory.subdirs.keys()):
            self._remove_directory_recursive(directory.subdirs[dname])
            del directory.subdirs[dname]

    def _find_recursive(self, directory, pattern, results, current_path):
        for fname, fobj in directory.files.items():
            if fnmatch.fnmatch(fname, pattern) or fnmatch.fnmatch(fobj.name, pattern):
                results.append(f"{current_path}/{fname}")

        for dname, subdir in directory.subdirs.items():
            new_path = f"{current_path}/{dname}" if current_path else f"/{dname}"
            if fnmatch.fnmatch(dname, pattern):
                results.append(new_path + "/")
            self._find_recursive(subdir, pattern, results, new_path)

    def _tree_entries(self, directory, prefix, lines):
        items = (
            [(n, "dir", d) for n, d in sorted(directory.subdirs.items())]
            + [(n, "file", o) for n, o in sorted(directory.files.items())]
        )
        for i, (name, kind, obj) in enumerate(items):
            is_last = i == len(items) - 1
            conn = "L-- " if is_last else "+-- "
            child_prefix = "    " if is_last else "|   "
            if kind == "dir":
                lines.append(f"{prefix}{conn}{name}/")
                self._tree_entries(obj, prefix + child_prefix, lines)
            else:
                lines.append(f"{prefix}{conn}{name}")

    def _copy_real_dir_to_virtual(self, real_path, overwrite=False):
        dirname = os.path.basename(real_path.rstrip("/\\"))
        try:
            self.MKDIR(dirname, overwrite=overwrite)
        except Exception as e:
            if "DIR_EXISTS" not in str(e):
                return f"Error creando directorio '{dirname}': {e}"

        saved_dir = self.current_dir
        self.current_dir = self.current_dir.subdirs[dirname]
        for item in os.listdir(real_path):
            item_path = os.path.join(real_path, item)
            if os.path.isfile(item_path):
                try:
                    with open(item_path, "r", encoding="utf-8", errors="replace") as fh:
                        content = fh.read()
                    name, ext = item.rsplit(".", 1) if "." in item else (item, "txt")
                    self.FILE(name, ext, content, overwrite=overwrite)
                except Exception:
                    pass
            elif os.path.isdir(item_path):
                self._copy_real_dir_to_virtual(item_path, overwrite)
        self.current_dir = saved_dir
        return f"Directorio '{dirname}' importado desde PC."

    def _copy_virtual_dir_to_real(self, vdir, real_dest):
        os.makedirs(real_dest, exist_ok=True)
        for fname, fobj in vdir.files.items():
            try:
                with open(os.path.join(real_dest, fname), "w", encoding="utf-8") as fh:
                    fh.write(fobj.content or "")
            except Exception:
                pass
        for dname, subdir in vdir.subdirs.items():
            self._copy_virtual_dir_to_real(subdir, os.path.join(real_dest, dname))
        return f"Directorio exportado a '{real_dest}'."

    def _copy_virtual_dir_to_virtual(self, source_dir, dest_name, overwrite=False):
        try:
            self.MKDIR(dest_name, overwrite=overwrite)
        except Exception as e:
            if "DIR_EXISTS" not in str(e):
                return f"Error: {e}"
        saved_dir = self.current_dir
        self.current_dir = self.current_dir.subdirs[dest_name]
        for fobj in source_dir.files.values():
            self.FILE(fobj.name, fobj.extension, fobj.content, overwrite=overwrite)
        for dname, subdir in source_dir.subdirs.items():
            self._copy_virtual_dir_to_virtual(subdir, dname, overwrite)
        self.current_dir = saved_dir
        return f"Directorio '{source_dir.name}' copiado como '{dest_name}'."
