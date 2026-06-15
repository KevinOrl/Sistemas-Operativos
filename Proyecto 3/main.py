import tkinter as tk
from filesystem.commands import FileSystem
from gui.interface import FileSystemGUI


def main():
    """Inicia la aplicación del File System Virtual con interfaz gráfica"""
    root = tk.Tk()
    fs = FileSystem()
    
    # Crear la interfaz gráfica
    app = FileSystemGUI(root, fs)
    
    # Iniciar la aplicación
    root.mainloop()


if __name__ == "__main__":
    main()
