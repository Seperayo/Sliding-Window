# :school: Tugas Besar Jaringan Komputer :school:  <!-- omit in toc -->

# To do List <!-- omit in toc -->
- [ ] **Refaktorisasi** dari main
- [ ] Implementasi **ACK** % **packet loss**
- [ ] Implementasi **Checksum** corrupt data
- [ ] Implementasi **delay** + **timeout**

# :maple_leaf: Table of Content  <!-- omit in toc -->
- [:busts_in_silhouette: Nama Anggota Kelompok](#busts_in_silhouette-nama-anggota-kelompok)
- [:computer: Penjelasan Penggunaan Program](#computer-penjelasan-penggunaan-program)
    - [Compiling Program](#compiling-program)
- [Penjelasan Sliding Window](#penjelasan-sliding-window)
- [Penjelasan Fungsi-Fungsi](#penjelasan-fungsi-fungsi)
- [Pembagian Tugas](#pembagian-tugas)
- [About](#about)

## :busts_in_silhouette: Nama Anggota Kelompok
:point_right: **Seperayo** -  **13516068**

:point_right: **Hafizh Budiman** - **13516137**

:point_right: **Ilham Firdausi Putra** - **13516140**

## :computer: Penjelasan Penggunaan Program
### Compiling Program
Pragram dapat di compile dengan menggunakan perintah **`make`** pada command line. :ledger: Gunakan ***Linux*** demi kompatibilitas library yang lebih baik :ledger:

```
all: recvfile sendfile

recvfile: src/config.h src/config.cpp src/recvfile.cpp
	g++ -pthread src/config.cpp src/recvfile.cpp -o recvfile -std=c++11 

sendfile: src/config.h src/config.cpp src/sendfile.cpp
	g++ -pthread src/config.cpp src/sendfile.cpp -o sendfile -std=c++11 

clean: recvfile sendfile
	rm -f src/recvfile src/sendfile
```

## Penjelasan Sliding Window
## Penjelasan Fungsi-Fungsi
## Pembagian Tugas
| Nama Anggota        | Tugas |
| ------------------- | ----- |
| Seperayo            |       |
| Ilham Firdaus Putra |       |
| Hafizh Budiman      |       |

## About