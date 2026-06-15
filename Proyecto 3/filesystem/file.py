import time
from datetime import datetime


class File:
    def __init__(self, name, extension, content, sectors):
        self.name = name
        self.extension = extension
        self.content = content
        self.sectors = sectors
        self.created_at = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.modified_at = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.size = len(content) if content else 0

    def modify(self, new_content):
        """Modifica el contenido del archivo"""
        self.content = new_content
        self.modified_at = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.size = len(new_content) if new_content else 0

    def get_info(self):
        """Retorna información del archivo"""
        return {
            'name': self.name,
            'extension': self.extension,
            'size': self.size,
            'created_at': self.created_at,
            'modified_at': self.modified_at,
            'sectors': self.sectors
        }
