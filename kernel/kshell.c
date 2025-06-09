/*
Copyright (C) 2016-2019 The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file LICENSE for details.

All modifiactions are copyrighted to XPDevs and James Turner
XPDevs and James Turner use basekernel as a base where mods are added and updated
for the NexShell kernel
*/

#include "kernel/types.h"
#include "kernel/error.h"
#include "kernel/ascii.h"
#include "kshell.h"
#include "console.h"
#include "string.h"
#include "rtc.h"
#include "kmalloc.h"
#include "page.h"
#include "process.h"
#include "main.h"
#include "fs.h"
#include "syscall_handler.h"
#include "clock.h"
#include "kernelcore.h"
#include "bcache.h"
#include "printf.h"
#include "graphics.h" // Include your graphics header

// define the start screen for when the gui starts
#define COLOR_BLUE  0x0000FF      // RGB hex for blue (blue channel max)
#define COLOR_WHITE 0xFFFFFF      // White color

// DO NOT TOUCH THESE
#define PM1a_CNT_BLK 0xB004      // Example ACPI PM1a control port (adjust per your ACPI)
#define SLP_TYP1    (0x5 << 10)  // Sleep type 1 for shutdown (example value)
#define SLP_EN      (1 << 13)

static int kshell_mount( const char *devname, int unit, const char *fs_type)
{
	if(current->ktable[KNO_STDDIR]) {
		printf("root filesystem already mounted, please unmount first\n");
		return -1;
	}

	struct device *dev = device_open(devname,unit);
	if(dev) {
		struct fs *fs = fs_lookup(fs_type);
		if(fs) {
			struct fs_volume *v = fs_volume_open(fs,dev);
			if(v) {
				struct fs_dirent *d = fs_volume_root(v);
				if(d) {
					current->ktable[KNO_STDDIR] = kobject_create_dir(d);
					return 0;
				} else {
					printf("mount: couldn't find root dir on %s unit %d!\n",device_name(dev),device_unit(dev));
					return -1;
				}
				fs_volume_close(v);
			} else {
				printf("mount: couldn't mount %s on %s unit %d\n",fs_type,device_name(dev),device_unit(dev));
				return -1;
			}
		} else {
			printf("mount: invalid fs type: %s\n", fs_type);
			return -1;
		}
		device_close(dev);
	} else {
		printf("mount: couldn't open device %s unit %d\n",devname,unit);
		return -1;
	}

	return -1;
}


static int kshell_printdir(const char *d, int length)
{
	while(length > 0) {
		printf("%s\n", d);
		int len = strlen(d) + 1;
		d += len;
		length -= len;
	}
	return 0;
}

static int kshell_listdir(const char *path)
{
	struct fs_dirent *d = fs_resolve(path);
	if(d) {
		int buffer_length = 1024;
		char *buffer = kmalloc(buffer_length);
		if(buffer) {
			int length = fs_dirent_list(d, buffer, buffer_length);
			if(length>=0) {
				kshell_printdir(buffer, length);
			} else {
				printf("list: %s is not a directory\n", path);
			}
			kfree(buffer);
		}
	} else {
		printf("list: %s does not exist\n", path);
	}

	return 0;
}

static int kshell_execute(int argc, const char **argv)
{
    if (argc < 1) {
        printf("No command provided.\n");
        return -1;
    }

    const char *cmd = argv[0];

// Very detailed help function for each command
void print_command_help(const char *command) {
    if (!strcmp(command, "start")) {
        printf("start <path> <args>\n");
        printf("This command begins (or 'starts') a program right away.\n");
        printf("It does NOT wait for the program to finish — it just runs it and lets you keep doing other things.\n");
        printf("<path> is where the program is located. For example, /bin/hello\n");
        printf("<args> are extra words you give to the program (optional), like settings.\n");
        printf("Example: start /bin/hello World\n");
        printf("Starts the 'hello' program with the word 'World' as input.\n\n");

    } else if (!strcmp(command, "run")) {
        printf("run <path> <args>\n");
        printf("Runs a program, just like 'start', but it waits until the program finishes.\n");
        printf("This is useful if you want to see what the program does before moving on.\n");
        printf("At the end, it tells you the result (called an 'exit status').\n");
        printf("Example: run /bin/update\n");
        printf("Starts the update program, waits for it to finish, and shows the result.\n\n");

    } else if (!strcmp(command, "list")) {
        printf("list <directory>\n");
        printf("Shows you all the files and folders in a certain location (called a directory).\n");
        printf("f you don't give it a directory, it will show you the root (main) folder.\n");
        printf("Example: list /home/user\n");
        printf("Shows files inside the '/home/user' folder.\n\n");

    } else if (!strcmp(command, "mount")) {
        printf("mount <device> <unit> <fstype>\n");
        printf("This connects (or 'mounts') a storage device (like a USB or hard drive) to the system.\n");
        printf("<device> is the name of the hardware, like 'sda' for a hard disk.\n");
        printf("<unit> is the number of the part (like partition 1, 2, etc).\n");
        printf("<fstype> is the type of file system (like FAT32, EXT4, etc).\n");
        printf("Example: mount sda 1 ext4\n");
        printf("Mounts the first part of the 'sda' disk as an EXT4 file system.\n\n");

    } else if (!strcmp(command, "kill")) {
        printf("kill <pid>\n");
        printf("Stops a running program by force.\n");
        printf("<pid> is the Process ID — a number the computer gives to each running program.\n");
        printf("You can find it by using the 'ps' or 'top' command if supported.\n");
        printf("Example: kill 123\n");
        printf("Stops the program with process ID 123.\n\n");

    } else if (!strcmp(command, "reboot")) {
        printf("reboot\n");
        printf("Restarts the entire system — just like pressing the restart button.\n");
        printf("It closes all programs and boots up fresh.\n\n");

    } else if (!strcmp(command, "shutdown")) {
        printf("shutdown\n");
        printf("Turns off the computer completely.\n");
        printf("Save your work before using this, as it will close everything.\n\n");

    } else if (!strcmp(command, "clear")) {
        printf("clear\n");
        printf("Wipes everything off the screen so you get a clean console.\n");
        printf("This doesn't delete files — it's just like clearing a whiteboard.\n\n");

    } else if (!strcmp(command, "neofetch")) {
        printf("neofetch\n");
        printf("Shows information about your system — like its name, version, memory, and more.\n");
        printf("Often includes a cool logo made from text!\n\n");

    } else if (!strcmp(command, "startGUI")) {
        printf("startGUI\n");
        printf("Turns on the graphical user interface (GUI).\n");
        printf("The GUI is the part of a computer with windows, buttons, and icons.\n");
        printf("This lets you interact with the computer visually instead of just typing.\n\n");

    } else if (!strcmp(command, "cowsay")) {
        printf("cowsay <message>\n");
        printf("A fun command that shows a cartoon cow saying something you type.\n");
        printf("Example: cowsay Hello!\n");
        printf("Shows a cow saying 'Hello!'. Just for laughs.\n\n");

    } else if (!strcmp(command, "help")) {
        printf("help [command]\n");
        printf("If used by itself (just 'help'), it shows a list of all available commands.\n");
        printf("If used with a command name (like 'help start'), it gives more details.\n\n");

    } else {
        printf("No detailed help available for '%s'.\n", command);
        printf("Please check the spelling or try just typing: help\n\n");
    }
}


    if (!strcmp(cmd, "start")) {
        if (argc > 1) {
            int fd = sys_open_file(KNO_STDDIR, argv[1], 0, 0);
            if (fd >= 0) {
                int pid = sys_process_run(fd, argc - 1, &argv[1]);
                if (pid > 0) {
                    printf("started process %d\n", pid);
                    process_yield();
                } else {
                    printf("couldn't start %s\n", argv[1]);
                }
                sys_object_close(fd);
            } else {
                printf("couldn't find %s\n", argv[1]);
            }
        } else {
            printf("start: requires argument.\n");
        }
    } else if (!strcmp(cmd, "run")) {
        if (argc > 1) {
            int fd = sys_open_file(KNO_STDDIR, argv[1], 0, 0);
            if (fd >= 0) {
                int pid = sys_process_run(fd, argc - 1, &argv[1]);
                if (pid > 0) {
                    printf("started process %d\n", pid);
                    process_yield();
                    struct process_info info;
                    process_wait_child(pid, &info, -1);
                    printf("process %d exited with status %d\n", info.pid, info.exitcode);
                    process_reap(info.pid);
                } else {
                    printf("couldn't start %s\n", argv[1]);
                }
                sys_object_close(fd);
            } else {
                printf("couldn't find %s\n", argv[1]);
            }
        } else {
            printf("run: requires argument\n");
        }
    } else if (!strcmp(cmd, "list")) {
        if (argc > 1) {
            printf("\nFiles of '%s'\n", argv[1]);
            kshell_listdir(argv[1]);
        } else {
            printf("\nFiles of '/'\n");
            kshell_listdir(".");
        }
    } else if (!strcmp(cmd, "mount")) {
        if (argc == 4) {
            int unit;
            if (str2int(argv[2], &unit)) {
                kshell_mount(argv[1], unit, argv[3]);
            } else {
                printf("mount: expected unit number but got %s\n", argv[2]);
            }
        } else {
            printf("mount: requires device, unit, and fs type\n");
        }
    } else if (!strcmp(cmd, "kill")) {
        if (argc > 1) {
            int pid;
            if (str2int(argv[1], &pid)) {
                process_kill(pid);
            } else {
                printf("kill: expected process id number but got %s\n", argv[1]);
            }
        } else {
            printf("kill: requires argument\n");
        }
	} else if(!strcmp(cmd, "mkdir")) {
		if(argc == 3) {
			struct fs_dirent *dir = fs_resolve(argv[1]);
			if(dir) {
				struct fs_dirent *n = fs_dirent_mkdir(dir,argv[2]);
				if(!n) {
					printf("mkdir: couldn't create %s\n",argv[2]);
				} else {
					fs_dirent_close(n);
				}
				fs_dirent_close(dir);
			} else {
				printf("mkdir: couldn't open %s\n",argv[1]);
			}
		} else {
			printf("use: mkdir <parent-dir> <dirname>\n");
		} 
} else if (!strcmp(cmd, "reboot")) {
        reboot();
   } else if (!strcmp(cmd, "shutdown")) {
    if (argc > 1 && !strcmp(argv[1], "cowsay")) {
        // User wants shutdown cowsay <message>
        if (argc > 2) {
            // Combine all args after "cowsay" into one message
            int total_len = 0;
            for (int i = 2; i < argc; i++) {
                total_len += strlen(argv[i]) + 1; // space or terminator
            }

            char *msg = kmalloc(total_len);
            if (!msg) {
                printf("shutdown cowsay: memory allocation failed.\n");
                return -1;
            }

            msg[0] = '\0';
            for (int i = 2; i < argc; i++) {
                strcat(msg, argv[i]);
                if (i < argc - 1) strcat(msg, " ");
            }

            cowsay(msg);
            kfree(msg);
        } else {
            printf("Usage: shutdown cowsay <message>\n");
            return -1;
        }
    }
    // Then proceed with shutdown normally
    shutdown_user();
} else if (!strcmp(cmd, "clear")) {
        clear();
    } else if (!strcmp(cmd, "neofetch")) {
        neofetch();
    } else if (!strcmp(cmd, "startGUI")) {
        GUI();
    } else if (!strcmp(cmd, "automount")) {
        automount();
} else if (!strcmp(cmd, "unmount")) {
if (current->ktable[KNO_STDDIR]) {
        printf("\nunmounting root directory\n");
        sys_object_close(KNO_STDDIR);
    } else {
        printf("\nnothing currently mounted\n");
    }
} else if (!strcmp(cmd, "cowsay")) {
    if (argc > 1) {
        // Calculate total length for the message
        int total_len = 0;
        for (int i = 1; i < argc; i++) {
            total_len += strlen(argv[i]) + 1; // +1 for space or null terminator
        }

        char *msg = kmalloc(total_len);
        if (!msg) {
            printf("cowsay: memory allocation failed.\n");
            return -1;
        }

        msg[0] = '\0';
        for (int i = 1; i < argc; i++) {
            strcat(msg, argv[i]);
            if (i < argc - 1) strcat(msg, " ");
        }

        cowsay(msg);
        kfree(msg);
    } else {
        printf("Usage: cowsay <message>\n");
    }
} else if (!strcmp(cmd, "contents")) {
    if (argc > 1) {
        const char *filepath = argv[1];
        printf("Reading file: %s\n", filepath);

        int fd = sys_open_file(KNO_STDDIR, filepath, 0, 0);
        if (fd < 0) {
            printf("Failed to open file: %s\n", filepath);
            return 0;
        }

        char *buffer = kmalloc(4096);
        if (!buffer) {
            printf("Memory allocation failed\n");
            sys_object_close(fd);
            return 0;
        }

        int bytes_read = sys_object_read(fd, buffer, 4096);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("\f%s\n", buffer);  // Clear screen before showing contents
        } else {
            printf("File read failed or is empty\n");
        }

        kfree(buffer);
        sys_object_close(fd);

        __asm__ __volatile__ (
            "mov $100000000, %%ecx\n"
            "1:\n"
            "dec %%ecx\n"
            "jnz 1b\n"
            :
            :
            : "ecx"
        );
    } else {
        printf("Usage: contents <filepath>\n");
    } 
} else if (!strcmp(cmd, "help")) {
           if (argc == 1) {
        printf("\nCommands:\n");
        printf("start <path> <args>\n");
        printf("run <path> <args>\n");
        printf("list <directory>\n");
        printf("mount <device> <unit> <fstype>\n");
        printf("kill <pid>\n");
        printf("reboot\n");
        printf("shutdown\n");
        printf("clear\n");
        printf("neofetch\n");
        printf("startGUI\n");
        printf("automount\n");
        printf("unmount\n");
        printf("help <command>\n");
        printf("contents <file>");
        printf("cowsay\n\n");
} else if (argc == 2) {
            print_command_help(argv[1]);
        } else {
            printf("Usage: help [command]\n");
        }
    } else {
        printf("%s: command not found                                                                       :(\n", argv[0]);
    }

    return 0;
}



int kshell_readline(char *line, int length)
{
	int i = 0;
	while(i < (length - 1)) {
		char c = console_getchar(&console_root);
		if(c == ASCII_CR) {
			line[i] = 0;
			printf("\n");
			return 1;
		} else if(c == ASCII_BS) {
			if(i > 0) {
				putchar(c);
				i--;
			}
		} else if(c >= 0x20 && c <= 0x7E) {
			putchar(c);
			line[i] = c;
			i++;
		}
	}

	return 0;
}

void cowsay(const char *message) {
    int len = strlen(message);
    int i;

    // Top of bubble
    printf(" ");
    for (i = 0; i < len + 2; i++) printf("_");
    printf("\n");

    // Bubble with message
    printf("< %s >\n", message);

    // Bottom of bubble
    printf(" ");
    for (i = 0; i < len + 2; i++) printf("-");
    printf("\n");

    // Cow ASCII art
    printf("        \\   ^__^\n");
    printf("         \\  (oo)\\_______\n");
    printf("            (__)\\       )\\/\\\n");
    printf("                ||----w |\n");
    printf("                ||     ||\n");
}


////////////////////////////////////////////////////////////
// everything past this point is for system interactions //
//////////////////////////////////////////////////////////


int automount()
{
	int i;

	for(i=0;i<4;i++) {
		printf("automount: trying atapi unit %d.\n",i);
		if(kshell_mount("atapi",i,"cdromfs")==0) return 0;
	}

	for(i=0;i<4;i++) {
		printf("automount: trying ata unit %d.\n",i);
		if(kshell_mount("ata",i,"simplefs")==0) return 0;
	}

	printf("automount: no bootable devices available.\n");
	return -1;
}

void shutdown_user() {
    clear();
    // main message for shutdown
    printf("Powering off... ");
    // inside this will be containing all of the background processes of shutting down
    // that will consist of stopping all the background processes, unmounting disk and rootfs
    // as well as then running the acpi command to poweroff the cpu
    // if the computer cant be shutdown using the acpi it warns the user and halts the system for it to force powered off

   // first process is to kill them all as well as the children
    for (int pid = 2; pid <= 100; pid++) {
        process_kill(pid);
        if ((pid - 1) % 10 == 0) {
        }
    }

   // unmount the disk and root file system
if (current->ktable[KNO_STDDIR]) {
        sys_object_close(KNO_STDDIR);
    } else {
    }


    // wait to show the user it had been proceesing stuff
        __asm__ __volatile__ (
        "mov $400000000, %%ecx\n"
        "1:\n"
        "dec %%ecx\n"
        "jnz 1b\n"
        :
        :
        : "ecx"
    );

// print that it was successful of shutting it down just before the acpi
printf("Done\n");

    // wait to make it smooth instead of the rush
        __asm__ __volatile__ (
        "mov $400000000, %%ecx\n"
        "1:\n"
        "dec %%ecx\n"
        "jnz 1b\n"
        :
        :
        : "ecx"
    );


    // acpi command to poweroff the cpu
    __asm__ __volatile__ (
        "mov $0x604, %%dx\n\t"
        "mov $0x2000, %%ax\n\t"
        "out %%ax, %%dx\n\t"
        :
        :
        : "ax", "dx"
    );

// make the user force poweroff the system if it can be turned off by the acpi
// display the warning message and then halt the cpu

    clear();
    printf("System halted.\n");
    printf("The system could not be shut down via ACPI.\n");

    halt();
}

int GUI() {
    printf("\nThe GUI is being loaded, please wait during this time, as it may take a while\n");

    // Simple delay loop for realism
    __asm__ __volatile__ (
        "mov $400000000, %%ecx\n"
        "1:\n"
        "dec %%ecx\n"
        "jnz 1b\n"
        :
        :
        : "ecx"
    );

    // Try to open the GUI file
    int fd = sys_open_file(KNO_STDDIR, "/core/gui/main.nex", 0, 0);
    if (fd < 0) {
        printf("GUI: Failed to open core/gui/main.nex\n");
        return -1;
    }

    // Read contents into a buffer
    char *buffer = kmalloc(4096);  // Allocate 4KB for GUI data
    if (!buffer) {
        printf("GUI: Memory allocation failed\n");
        sys_object_close(fd);
        return -1;
    }

    int bytes_read = sys_object_read(fd, buffer, 4096);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; // Null-terminate
        printf("\f%s\n", buffer);
    } else {
        printf("GUI: File read failed or is empty\n");
    }

    // Cleanup
    kfree(buffer);
    sys_object_close(fd);


    // Simple delay loop for realism
    __asm__ __volatile__ (
        "mov $400000000, %%ecx\n"
        "1:\n"
        "dec %%ecx\n"
        "jnz 1b\n"
        :
        :
        : "ecx"
    );

    return 0;

}


void neofetch() {
    const char *architecture = "x86";
    const char *shell = "Kshell";

    printf("\n");
    printf("|----------------------------------------------------------|\n");
    printf("|                     NexShell v1.2.3                      |\n");
    printf("|                  (C)Copyright 2025 XPDevs                |\n");
    printf("|                  Build id: NS127-0425-S1                 |\n");
    printf("|----------------------------------------------------------|\n");
    printf("| Architecture: %s\n", architecture);
    printf("| Shell: %s\n", shell);
    printf("| video: %d x %d\n", video_xres, video_yres, video_buffer);
    printf("|----------------------------------------------------------|\n\n");
}

void clear() {
printf("\f\n");
}


int kshell_launch()
{
	char line[1024];
	const char *argv[100];
	int argc;
// everything past here for control

printf("acpi: Installed for shutdown\n\n");

    // Try to mount the root filesystem
    printf("Mounting root filesystem\n");

automount();
neofetch(); // print system info

// go straight into the GUI but command line if it has any errors
GUI();

	while(1) {
               printf("\n");
		printf("root@Doors: /core/NexShell# ");
		kshell_readline(line, sizeof(line));

		argc = 0;
		argv[argc] = strtok(line, " ");
		while(argv[argc]) {
			argc++;
			argv[argc] = strtok(0, " ");
		}

		if(argc > 0) {
			kshell_execute(argc, argv);
		}
	}
	return 0;
}
