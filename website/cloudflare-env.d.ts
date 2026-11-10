declare namespace Cloudflare {
  interface Env {
    DB?: D1Database;
    BUCKET?: R2Bucket;
    HITMAN_PUBLISH_TOKEN_SHA256?: string;
  }
}
