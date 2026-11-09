// Browser demonstrations never call the lobby API or access local files.
export function initializeHub() {
  const modal = document.getElementById("preview-modal") as HTMLDialogElement;
  const card = document.getElementById("launcher-mockup")!;
  const join = document.getElementById("demo-join-input") as HTMLInputElement;
  const log = document.getElementById("sim-log")!;
  const downloadText = document.getElementById("download-btn-text")!;
  const timers = new Set<ReturnType<typeof setTimeout>>();
  let returnFocus: HTMLElement | null = null;
  let frame = 0;
  let pendingTransform = "";
  const later = (callback: () => void, delay: number) => {
    const id = setTimeout(() => { timers.delete(id); callback(); }, delay);
    timers.add(id);
  };
  const append = (message: string) => {
    const line = document.createElement("div");
    line.textContent = "[DEMO] " + message;
    log.appendChild(line);
    while (log.childElementCount > 35) log.firstElementChild?.remove();
    log.scrollTop = log.scrollHeight;
  };
  const filter = (category: string) => {
    let count = 0;
    document.querySelectorAll<HTMLElement>(".mod-item").forEach(item => {
      const visible = category === "all" || item.dataset.category === category;
      item.hidden = !visible;
      item.style.display = visible ? "flex" : "none";
      if (visible) count++;
    });
    document.querySelectorAll<HTMLElement>(".filter-tab").forEach(tab => {
      const selected = tab.dataset.filter === category;
      tab.classList.toggle("bg-brand-orange", selected);
      tab.classList.toggle("text-black", selected);
      tab.classList.toggle("text-slate-400", !selected);
      tab.setAttribute("aria-pressed", String(selected));
    });
    document.getElementById("catalog-status")!.textContent = `${count} project${count === 1 ? "" : "s"} shown`;
  };
  const onClose = () => {
    document.body.style.overflow = "";
    returnFocus?.focus();
    timers.forEach(clearTimeout); timers.clear();
    downloadText.textContent = "Download for Windows (.zip)";
  };
  const onClick = (event: MouseEvent) => {
    const target = event.target instanceof Element ? event.target : null;
    const control = target?.closest<HTMLElement>("[data-action], [data-filter], #hero-download-btn");
    if (!control) return;
    if (control.dataset.filter) { filter(control.dataset.filter); return; }
    if (control.id === "hero-download-btn") {
      downloadText.textContent = "Download requested · check your browser";
      later(() => { downloadText.textContent = "Download for Windows (.zip)"; }, 5000);
      return;
    }
    switch (control.dataset.action) {
      case "open":
        returnFocus = control;
        modal.showModal();
        document.body.style.overflow = "hidden";
        break;
      case "close": modal.close(); break;
      case "browse":
        append("Example game folder: C:\\Games\\Stray. Use Browse in the desktop launcher to select your installation.");
        break;
      case "host":
        append("Simulating session creation on UDP 7777…");
        later(() => append("Example lobby: CAT2PAWS. Demo only — this is not a real join code."), 450);
        break;
      case "join": {
        const code = join.value.replace(/[\s-]/g, "").toUpperCase();
        if (!/^[23456789ABCDEFGHJKLMNPQRSTUVWXYZ]{8}$/.test(code)) {
          join.setAttribute("aria-invalid", "true");
          append("Enter an eight-character example code, such as CAT2PAWS.");
          join.focus();
          break;
        }
        join.removeAttribute("aria-invalid");
        append(`Simulating connection to ${code}…`);
        later(() => append("Demo connected. In the desktop app, Stray starts automatically after both players connect."), 500);
        break;
      }
    }
  };
  const onKey = (event: KeyboardEvent) => {
    if (event.target === card && (event.key === "Enter" || event.key === " ")) {
      event.preventDefault(); card.click();
    }
    if (event.target === join && event.key === "Enter") {
      modal.querySelector<HTMLButtonElement>('[data-action="join"]')?.click();
    }
  };
  const onBackdrop = (event: MouseEvent) => { if (event.target === modal) modal.close(); };
  const onMove = (event: MouseEvent) => {
    if (!matchMedia("(hover: hover) and (prefers-reduced-motion: no-preference)").matches) return;
    const box = card.getBoundingClientRect();
    const x = (event.clientX - box.left - box.width / 2) / box.width;
    const y = (event.clientY - box.top - box.height / 2) / box.height;
    pendingTransform = `rotateY(${x * 8}deg) rotateX(${-y * 8}deg)`;
    if (!frame) frame = requestAnimationFrame(() => { card.style.transform = pendingTransform; frame = 0; });
  };
  const onLeave = () => { cancelAnimationFrame(frame); frame = 0; card.style.transform = ""; };
  filter("all");
  document.addEventListener("click", onClick);
  document.addEventListener("keydown", onKey);
  modal.addEventListener("click", onBackdrop);
  modal.addEventListener("close", onClose);
  card.addEventListener("mousemove", onMove);
  card.addEventListener("mouseleave", onLeave);
  return () => {
    document.removeEventListener("click", onClick);
    document.removeEventListener("keydown", onKey);
    modal.removeEventListener("click", onBackdrop);
    modal.removeEventListener("close", onClose);
    card.removeEventListener("mousemove", onMove);
    card.removeEventListener("mouseleave", onLeave);
    timers.forEach(clearTimeout); cancelAnimationFrame(frame);
    document.body.style.overflow = "";
  };
}
