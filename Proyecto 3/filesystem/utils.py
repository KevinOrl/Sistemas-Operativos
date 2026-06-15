"""Funciones de utilidad para el File System"""

def get_path_components(path):
    """Divide una ruta en componentes"""
    if path == "/" or path == "":
        return []
    if path.startswith("/"):
        path = path[1:]
    if path.endswith("/"):
        path = path[:-1]
    return path.split("/")

def format_size(size_bytes):
    """Formatea el tamaño en bytes a formato legible"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size_bytes < 1024:
            return f"{size_bytes:.2f} {unit}"
        size_bytes /= 1024
    return f"{size_bytes:.2f} TB"

def match_pattern(name, pattern):
    """Coincidencia simple con patrón (*.ext)"""
    if pattern == "*":
        return True
    if pattern.startswith("*"):
        return name.endswith(pattern[1:])
    if pattern.endswith("*"):
        return name.startswith(pattern[:-1])
    return name == pattern

def get_full_path(directory, root):
    """Obtiene la ruta completa de un directorio"""
    path = []
    current = directory
    while current is not None and current != root:
        path.insert(0, current.name)
        current = current.parent
    return "/" + "/".join(path) if path else "/"
