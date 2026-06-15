import os
import time

class Sector:
    def __init__(self, size):
        self.size = size
        self.data = None
        self.next = None  # enlace al siguiente sector
        self.is_allocated = False

class VirtualDisk:
    def __init__(self, num_sectors, sector_size, filename="virtual_disk.bin"):
        self.num_sectors = num_sectors
        self.sector_size = sector_size
        self.filename = filename
        self.sectors = [Sector(sector_size) for _ in range(num_sectors)]
        self.free_list = list(range(num_sectors))  # lista de sectores libres
        self.allocation_table = {}  # tabla de asignación (sector -> next_sector or -1)

        # Crear archivo físico del disco
        with open(self.filename, "wb") as f:
            f.write(b'\x00' * (num_sectors * sector_size))

    def allocate(self, num_needed):
        """Asignación First Fit con enlace"""
        if len(self.free_list) < num_needed:
            raise Exception(f"Disco lleno. Se necesitan {num_needed} sectores pero hay {len(self.free_list)} disponibles.")

        allocated = []
        for i in range(num_needed):
            sector_index = self.free_list.pop(0)
            allocated.append(sector_index)
            self.sectors[sector_index].is_allocated = True
            
            # Crear enlace al siguiente sector
            if i < num_needed - 1:
                self.allocation_table[sector_index] = allocated[i + 1]
            else:
                self.allocation_table[sector_index] = -1  # fin de la cadena
        
        return allocated

    def free(self, sector_indices):
        """Liberar sectores"""
        for idx in sector_indices:
            if idx >= 0 and idx < self.num_sectors:
                if self.sectors[idx].is_allocated:
                    self.sectors[idx].is_allocated = False
                    if idx not in self.free_list:
                        self.free_list.append(idx)
        self.free_list.sort()

    def get_disk_status(self):
        """Obtiene el estado del disco"""
        used = len([s for s in self.sectors if s.is_allocated])
        free = len(self.free_list)
        total = self.num_sectors
        percentage = (used / total) * 100 if total > 0 else 0
        
        return {
            'total': total,
            'used': used,
            'free': free,
            'size_bytes': self.num_sectors * self.sector_size,
            'percentage': percentage
        }

    def save_to_disk(self):
        """Guarda la información del disco en el archivo"""
        # Esta función puede ser expandida para serializar metadatos
        pass

    def load_from_disk(self):
        """Carga la información del disco desde el archivo"""
        # Esta función puede ser expandida para deserializar metadatos
        pass
