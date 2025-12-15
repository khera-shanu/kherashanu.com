import { Link, Outlet, useLocation } from "react-router-dom"
import { PROFILE } from "@/data/profile"
import { Github, Linkedin } from "lucide-react"
import { Button } from "../ui/button"

function Navbar() {
    return (
        <header className="sticky top-0 z-50 w-full border-b bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60">
            <div className="container flex h-14 items-center justify-between">
                <div className="mr-4 flex">
                    <Link to="/" className="mr-6 flex items-center space-x-2">
                        <span className="font-bold text-xl">{PROFILE.basics.name}</span>
                    </Link>
                    <nav className="flex items-center space-x-6 text-sm font-medium">
                        <Link
                            to="/"
                            className="transition-colors hover:text-foreground/80 text-foreground/60 hover:underline"
                        >
                            Portfolio
                        </Link>
                        <Link
                            to="/blog"
                            className="transition-colors hover:text-foreground/80 text-foreground/60 hover:underline"
                        >
                            Blog
                        </Link>
                    </nav>
                </div>
                <div className="flex items-center space-x-2">
                    {PROFILE.sections.profiles.items.map((profile) => (
                        <Button
                            key={profile.network}
                            variant="ghost"
                            size="icon"
                            asChild
                        >
                            <a href={profile.url.href} target="_blank" rel="noreferrer">
                                {profile.network === "Github" && <Github className="h-5 w-5" />}
                                {profile.network === "LinkedIn" && <Linkedin className="h-5 w-5" />}
                            </a>
                        </Button>
                    ))}
                </div>
            </div>
        </header>
    )
}

function Footer() {
    return (
        <footer className="border-t py-6 md:px-8 md:py-0">
            <div className="container flex flex-col items-center justify-between gap-4 md:h-24 md:flex-row">
                <p className="text-center text-sm leading-loose text-muted-foreground md:text-left">
                    Built by {PROFILE.basics.name}. The source code is available on{" "}
                    <a
                        href="https://github.com/khera-shanu/kherashanu.com"
                        target="_blank"
                        rel="noreferrer"
                        className="font-medium underline underline-offset-4"
                    >
                        GitHub
                    </a>
                    .
                </p>
            </div>
        </footer>
    )
}

export default function Layout() {
    const location = useLocation()
    const isHomePage = location.pathname === "/"

    return (
        <div className="min-h-screen bg-background font-sans antialiased flex flex-col">
            {!isHomePage && <Navbar />}
            <main className="flex-1">
                <Outlet />
            </main>
            <Footer />
        </div>
    )
}
