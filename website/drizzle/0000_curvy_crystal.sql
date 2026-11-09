CREATE TABLE `lobbies` (
	`code` text PRIMARY KEY NOT NULL,
	`owner_hash` text NOT NULL,
	`ip` text NOT NULL,
	`ip_hash` text NOT NULL,
	`port` integer NOT NULL,
	`session_key` text NOT NULL,
	`version` text NOT NULL,
	`protocol` integer NOT NULL,
	`expires` integer NOT NULL,
	`created` integer NOT NULL
);
--> statement-breakpoint
CREATE TABLE `rate_limits` (
	`id` text PRIMARY KEY NOT NULL,
	`count` integer NOT NULL,
	`expires` integer NOT NULL
);
