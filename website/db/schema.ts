import { integer, sqliteTable, text } from "drizzle-orm/sqlite-core";
export const lobbies=sqliteTable("lobbies",{
 code:text("code").primaryKey(),ownerHash:text("owner_hash").notNull(),
 ip:text("ip").notNull(),ipHash:text("ip_hash").notNull(),port:integer("port").notNull(),
 sessionKey:text("session_key").notNull(),version:text("version").notNull(),protocol:integer("protocol").notNull(),
 expires:integer("expires").notNull(),created:integer("created").notNull(),
});
export const rateLimits=sqliteTable("rate_limits",{
 id:text("id").primaryKey(),count:integer("count").notNull(),expires:integer("expires").notNull(),
});
