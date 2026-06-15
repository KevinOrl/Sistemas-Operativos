"""Widget especializado para mostrar el árbol del File System"""
import tkinter as tk
from tkinter import ttk


class FileTreeView(ttk.Treeview):
    """Widget que muestra la estructura de archivos y directorios"""
    
    def __init__(self, parent, filesystem, **kwargs):
        super().__init__(parent, **kwargs)
        self.fs = filesystem
        self.refresh()
    
    def refresh(self):
        """Actualiza el árbol con la estructura actual del File System"""
        # Limpiar árbol
        for item in self.get_children():
            self.delete(item)
        
        # Construir el árbol desde la raíz
        if self.fs.root:
            self._build_tree(self.fs.root, "")
            # Expandir el nodo raíz
            items = self.get_children()
            if items:
                self.item(items[0], open=True)
    
    def _build_tree(self, directory, parent):
        """Construye recursivamente el árbol de directorios"""
        # Crear nodo para el directorio actual
        dir_icon = "📁"
        node_id = self.insert(parent, "end", text=f"{dir_icon} {directory.name}/", 
                             tags=("directory",))
        
        # Insertar subdirectorios primero
        for dirname, subdir in sorted(directory.subdirs.items()):
            self._build_tree(subdir, node_id)
        
        # Insertar archivos
        for filename, file_obj in sorted(directory.files.items()):
            file_icon = "📄"
            self.insert(node_id, "end", 
                       text=f"{file_icon} {filename}.{file_obj.extension} ({file_obj.size}B)",
                       tags=("file",))
    
    def get_selected_item_info(self):
        """Obtiene información del elemento seleccionado"""
        selected = self.selection()
        if not selected:
            return None
        
        item_id = selected[0]
        item_text = self.item(item_id)['text']
        parent_id = self.parent(item_id)
        
        # Remover íconos
        if "📁" in item_text:
            name = item_text.replace("📁 ", "").replace("/", "")
            return {"type": "directory", "name": name}
        elif "📄" in item_text:
            name = item_text.replace("📄 ", "").split(" (")[0]
            return {"type": "file", "name": name.split(".")[0]}
        
        return None
