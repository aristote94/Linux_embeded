# COMPTE RENDU – TP Linux Embarqué – VEEK-MT2S (DE10-Standard)

## Auteurs

* Adrien Lascar
* Pierre Mirouze

---

## Introduction

Ce compte rendu détaille les travaux réalisés dans le cadre du TP de Linux embarqué sur la carte VEEK-MT2S. Ce TP avait pour objectif de mettre en œuvre les concepts vus en cours et en TD, notamment :

* Le développement de modules noyau Linux
* L’interaction avec l’espace utilisateur (via `/proc` et `/dev`)
* L’accès direct au matériel (via `ioremap`, `mmap`)
* L’utilisation du Device Tree
* La mise en œuvre d’un projet d’affichage sur afficheurs 7 segments

---

## 2 Modules kernel (TP2)

### 2.1 Accès aux registres

Un programme en espace utilisateur a été développé pour accéder directement aux registres mémoire de la carte via `mmap()`, en particulier à l’adresse `0xFF203000` (GPIO).

Cette méthode permet une mise en œuvre rapide, mais présente des limites :

* Non portable (liée à une adresse physique fixe)
* Moins sécurisé (pas de contrôle par le noyau)
* Non maintenable à long terme (contraire aux pratiques Linux)

---

### 2.2 Compilation de module noyau sur la VM

#### Question 1 – Création d’un module noyau simple

Un premier module a été créé avec les fonctions `__init` et `__exit`. Il affiche un message via `printk()` lors du chargement et déchargement.

```c
static int __init mon_module_init(void) {
    printk(KERN_INFO "Driver enregistre\n");
    return 0;
}
```

Ce module a été compilé sur la VM à l’aide du noyau source Terasic (`linux-socfpga`) et testé sur la carte via `insmod`, `rmmod`, et vérifié avec `dmesg`.

#### Question 2 – Enregistrement d’un driver caractère

Nous avons ajouté un driver caractère via `register_chrdev()` et une structure `file_operations` :

```c
register_chrdev(240, "le_driver", &fops);
```

Les fonctions `.open` et `.release` génèrent des messages dans `dmesg` pour vérifier l’appel des fonctions.

---

### 2.3 Cross-compilation de modules noyau

Nous avons réalisé la compilation croisée depuis la VM, en utilisant les outils `arm-linux-gnueabihf-` et les en-têtes du noyau extraits depuis la carte. Le fichier `/proc/config.gz` a été récupéré, décompressé, puis renommé en `.config`.

Les commandes suivantes ont été utilisées pour préparer la compilation :

```bash
export CROSS_COMPILE=/usr/bin/arm-linux-gnueabihf-
export ARCH=arm
make prepare
make scripts
```

Ensuite, les modules ont été compilés avec `make`.

---

### 2.4 Driver noyau pour LEDs avec accès matériel et /proc

#### Question 1 – Écriture d’un driver simple

Un module de type platform driver a été développé pour piloter les LEDs connectées à l’IP "dev,ensea", déclarée dans le Device Tree. Le mapping des registres a été fait via :

```c
dev->regs = devm_ioremap_resource(&pdev->dev, r);
```

L’écriture dans le registre s’effectue avec `iowrite32()`.

Une interface utilisateur `/dev/ensea_leds` a été créée pour permettre la lecture et l’écriture de l’état des LEDs.

#### Question 2 – Ajout d’un chenillard configurable

Le module a été enrichi d’un timer noyau (`mod_timer`) pour créer un chenillard.

* La **vitesse** du balayage est configurable via `/proc/ensea/speed`
* Le **sens** du chenillard est modifiable via `/proc/ensea/dir`
* Le **pattern courant** est modifiable par écriture dans `/dev/ensea_leds`, et lisible depuis ce même fichier

Le timer utilise `jiffies` et `msecs_to_jiffies()` pour le timing.

---

### 2.5 Intégration du module dans le Device Tree

Un nœud a été ajouté dans le fichier `.dts` pour déclarer notre périphérique :

```dts
ensea_leds@0xff200000 {
    compatible = "dev,ensea";
    reg = <0xff200000 0x10>;
};
```

Le driver utilise `of_match_table` pour détecter automatiquement ce nœud à l'initialisation. Le fichier `.dtb` a été recompilé avec `dtc` et copié sur la partition `/mntboot` de la carte.

---

## 4 Petit projet : Afficheurs 7 segments

### 4.1 Prise en main

Un premier module (`7SEG.c`) a été développé pour écrire une valeur fixe sur les afficheurs 7 segments (appelés "HEX"). L’adresse de base a été obtenue depuis le fichier `fpga.cpp` fourni par Terasic.

Les segments sont commandés par écriture mémoire directe :

```c
iowrite32(szMask[i], hex_base + 4 * i);
```

### 4.2 Affichage de l’heure (`7SEGHeure.c`)

Un module noyau avec timer affiche chaque seconde l’heure du système (format HHMMSS) sur les afficheurs. L’heure est obtenue via :

```c
ktime_get_real_ts64(&ts);
time_to_tm(ts.tv_sec, 0, &tm);
```

Les six chiffres sont affichés en miroir. La mise à jour se fait automatiquement toutes les secondes grâce à un timer périodique.

#### Remarque

Une tentative a été faite pour synchroniser avec l’heure réelle (RTC ou Internet), mais cette heure ne se reflète pas directement dans le noyau ; seule l’interface graphique LXDE de la carte semblait être impactée.

---

## Conclusion

Ce TP a permis de mettre en pratique des notions avancées de Linux embarqué :

* Écriture de modules noyau simples et complexes (drivers, timers)
* Interaction avec l’espace utilisateur via `/dev` et `/proc`
* Cross-compilation de modules noyau
* Utilisation du Device Tree pour l’abstraction matérielle
* Programmation de périphériques réels (LEDs, afficheurs 7 segments)
* Déploiement et test sur une plateforme FPGA SoC réelle
