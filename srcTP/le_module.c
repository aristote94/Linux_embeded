#include <linux/init.h>    // Pour les macros __init et __exit
#include <linux/module.h>  // Nécessaire pour tous les modules du noyau
#include <linux/kernel.h>  // Pour la fonction printk()
#include <linux/fs.h>      // Pour register_chrdev() et unregister_chrdev()

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Votre Nom");
MODULE_DESCRIPTION("Un module simple avec struct file_operations");
MODULE_VERSION("1.0");

#define MAJOR_NUM 240 // Numéro majeur choisi (doit être libre)

// Déclaration des fonctions pour les opérations sur le fichier
static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device ouvert\n");
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device ferme\n");
    return 0;
}

// Définition de la structure file_operations
static struct file_operations fops = {
    .open = device_open,
    .release = device_release,
};

// Fonction appelée lors du chargement du module
static int __init mon_module_init(void) {
    int result;

    // Enregistrement du driver avec fops
    result = register_chrdev(MAJOR_NUM, "le_driver", &fops);
    if (result < 0) {
        printk(KERN_ALERT "Echec de l'enregistrement du driver avec le numero majeur %d\n", MAJOR_NUM);
        return result;
    }

    printk(KERN_INFO "Driver enregistre avec succes avec le numero majeur %d\n", MAJOR_NUM);
    return 0; // Retourne 0 pour indiquer un chargement réussi
}

// Fonction appelée lors du déchargement du module
static void __exit mon_module_exit(void) {
    // Désenregistrement du driver
    unregister_chrdev(MAJOR_NUM, "le_driver");
    printk(KERN_INFO "Driver desenregistre avec succes\n");
}

// Déclaration des fonctions d'initialisation et de nettoyage
module_init(mon_module_init);
module_exit(mon_module_exit);