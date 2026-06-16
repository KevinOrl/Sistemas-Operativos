class Sector:
    def __init__(self, size):
        self.size = size
        self.data = None
        self.next = None
        self.is_allocated = False


class VirtualDisk:
    def __init__(self, num_sectors, sector_size, filename="virtual_disk.bin"):
        self.num_sectors = num_sectors
        self.sector_size = sector_size
        self.filename = filename
        self.sectors = [Sector(sector_size) for _ in range(num_sectors)]
        self.free_list = list(range(num_sectors))
        self.allocation_table = {}

        with open(self.filename, "wb") as f:
            f.write(b'\x00' * (num_sectors * sector_size))

    def allocate(self, num_needed):
        """First Fit allocation: collect all sectors first, then link them."""
        if len(self.free_list) < num_needed:
            raise Exception(
                f"Disco lleno. Se necesitan {num_needed} sectores pero solo hay "
                f"{len(self.free_list)} disponibles."
            )

        allocated = []
        for _ in range(num_needed):
            idx = self.free_list.pop(0)
            self.sectors[idx].is_allocated = True
            allocated.append(idx)

        for i, idx in enumerate(allocated):
            self.allocation_table[idx] = allocated[i + 1] if i < len(allocated) - 1 else -1

        return allocated

    def write_data(self, sector_indices, data):
        """Write data across the given sectors in the virtual disk file."""
        if isinstance(data, str):
            data = data.encode("utf-8")
        with open(self.filename, "r+b") as f:
            for i, idx in enumerate(sector_indices):
                chunk = data[i * self.sector_size: (i + 1) * self.sector_size]
                chunk = chunk.ljust(self.sector_size, b'\x00')
                f.seek(idx * self.sector_size)
                f.write(chunk)
                self.sectors[idx].data = chunk

    def free(self, sector_indices):
        """Free sectors and zero them out in the disk file."""
        with open(self.filename, "r+b") as f:
            for idx in sector_indices:
                if 0 <= idx < self.num_sectors and self.sectors[idx].is_allocated:
                    self.sectors[idx].is_allocated = False
                    self.sectors[idx].data = None
                    if idx not in self.free_list:
                        self.free_list.append(idx)
                    f.seek(idx * self.sector_size)
                    f.write(b'\x00' * self.sector_size)
        self.free_list.sort()

    def get_disk_status(self):
        used = sum(1 for s in self.sectors if s.is_allocated)
        free = len(self.free_list)
        total = self.num_sectors
        percentage = (used / total * 100) if total > 0 else 0
        return {
            'total': total,
            'used': used,
            'free': free,
            'size_bytes': self.num_sectors * self.sector_size,
            'percentage': percentage,
        }
