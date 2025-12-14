import { useState, useEffect } from "react"
import { Link } from "react-router-dom"
import { PROFILE } from "@/data/profile"
import { Badge } from "@/components/ui/badge"
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card"
import { Button } from "@/components/ui/button"
import { ArrowRight, Github, Linkedin, Twitter, Youtube } from "lucide-react"
import { getLatestBlog, type BlogPost } from "@/lib/api"
import { MarkdownExcerpt } from "@/components/markdown/MarkdownExcerpt"

export default function HomePage() {
    const [latestBlog, setLatestBlog] = useState<BlogPost | null>(null)

    useEffect(() => {
        const fetchLatest = async () => {
            try {
                const blog = await getLatestBlog()
                setLatestBlog(blog)
            } catch {
                // No blogs yet - that's fine
            }
        }
        fetchLatest()
    }, [])

    return (
        <div className="container py-10 space-y-16 max-w-3xl mx-auto">
            {/* Hero Section */}
            <section className="flex flex-col items-center text-center space-y-8 py-16">
                <div className="relative shrink-0">
                    <div className="w-32 h-32 md:w-40 md:h-40 rounded-full overflow-hidden border-4 border-muted shadow-xl">
                        <img
                            src={PROFILE.basics.picture.url}
                            alt={PROFILE.basics.name}
                            className="w-full h-full object-cover"
                        />
                    </div>
                </div>

                <div className="space-y-4 max-w-2xl">
                    <h1 className="text-4xl md:text-5xl font-bold tracking-tighter">
                        {PROFILE.basics.name}
                    </h1>
                    <p className="text-xl text-muted-foreground">
                        {PROFILE.basics.headline}
                    </p>
                </div>

                <div className="flex gap-4">
                    {PROFILE.sections.profiles.items.map((profile) => {
                        let Icon = Github;
                        if (profile.network === "LinkedIn") Icon = Linkedin;
                        if (profile.network === "Twitter") Icon = Twitter;
                        if (profile.network === "Youtube") Icon = Youtube;

                        return (
                            <Button key={profile.network} variant="outline" size="icon" asChild>
                                <a href={profile.url.href} target="_blank" rel="noreferrer">
                                    <Icon className="h-4 w-4" />
                                </a>
                            </Button>
                        )
                    })}
                </div>

                <div className="flex gap-4 pt-4">
                    <Button size="lg" className="rounded-full px-8" asChild>
                        <Link to="/blog">About Me</Link>
                    </Button>
                    <Button variant="secondary" size="lg" className="rounded-full px-8" asChild>
                        <Link to="/blog">Blog</Link>
                    </Button>
                </div>
            </section>

            {/* Latest Blog Post Excerpt */}
            <section className="space-y-6">
                <h2 className="text-2xl font-bold tracking-tight text-center">Latest from the Blog</h2>
                {latestBlog ? (
                    <Link to={`/blog/${latestBlog.url_slug}`}>
                        <Card className="hover:shadow-md transition-shadow">
                            <CardHeader>
                                <CardTitle>{latestBlog.title}</CardTitle>
                                <CardDescription>
                                    Published on {new Date(latestBlog.publish_date * 1000).toLocaleDateString()}
                                </CardDescription>
                            </CardHeader>
                            <CardContent>
                                <MarkdownExcerpt content={latestBlog.summary || latestBlog.description || latestBlog.content} />
                            </CardContent>
                            <div className="p-6 pt-0 flex justify-end">
                                <Button variant="link" className="px-0">
                                    Read More <ArrowRight className="ml-2 h-4 w-4" />
                                </Button>
                            </div>
                        </Card>
                    </Link>
                ) : (
                    <Card>
                        <CardContent className="py-8 text-center text-muted-foreground">
                            No blog posts yet. Check back soon!
                        </CardContent>
                    </Card>
                )}
            </section>

            {/* About Section */}
            <section className="space-y-6">
                <h2 className="text-3xl font-bold tracking-tight">About Me</h2>
                <div
                    className="prose prose-stone dark:prose-invert max-w-none text-muted-foreground"
                    dangerouslySetInnerHTML={{ __html: PROFILE.sections.summary.content }}
                />
            </section>

            {/* Experience Section */}
            <section className="space-y-6">
                <h2 className="text-3xl font-bold tracking-tight">Experience</h2>
                <div className="space-y-8">
                    {PROFILE.sections.experience.items.map((job) => (
                        <div key={job.id} className="relative pl-8 border-l border-muted pb-8 last:pb-0">
                            <span className="absolute -left-[5px] top-2 h-2.5 w-2.5 rounded-full bg-primary" />
                            <div className="space-y-2">
                                <div className="flex flex-col sm:flex-row sm:items-center sm:justify-between">
                                    <h3 className="text-xl font-semibold">{job.position}</h3>
                                    <span className="text-sm text-muted-foreground bg-secondary px-2 py-1 rounded">
                                        {job.date}
                                    </span>
                                </div>
                                <p className="text-lg font-medium text-foreground/80">{job.company}</p>
                                <p className="text-sm text-muted-foreground">{job.location}</p>
                                <div
                                    className="prose prose-sm dark:prose-invert max-w-none text-muted-foreground mt-4"
                                    dangerouslySetInnerHTML={{ __html: job.summary }}
                                />
                            </div>
                        </div>
                    ))}
                </div>
            </section>

            {/* Skills Section */}
            <section className="space-y-6">
                <h2 className="text-3xl font-bold tracking-tight">Skills</h2>
                <div className="grid gap-6 md:grid-cols-2 lg:grid-cols-3">
                    {PROFILE.sections.skills.items.map((skillGroup) => (
                        <Card key={skillGroup.id}>
                            <CardHeader>
                                <CardTitle className="text-lg">{skillGroup.name}</CardTitle>
                            </CardHeader>
                            <CardContent className="flex flex-wrap gap-2">
                                {skillGroup.keywords.map((keyword) => (
                                    <Badge key={keyword} variant="secondary">
                                        {keyword}
                                    </Badge>
                                ))}
                            </CardContent>
                        </Card>
                    ))}
                </div>
            </section>

            {/* Projects Section */}
            <section className="space-y-6">
                <h2 className="text-3xl font-bold tracking-tight">Featured Projects</h2>
                <div className="grid gap-6 md:grid-cols-2">
                    {PROFILE.sections.projects.items.map((project) => (
                        <Card key={project.id} className="flex flex-col">
                            <CardHeader>
                                <CardTitle>{project.name}</CardTitle>
                                <CardDescription>{project.description}</CardDescription>
                            </CardHeader>
                            <CardContent className="flex-1">
                                <div
                                    className="prose prose-sm dark:prose-invert"
                                    dangerouslySetInnerHTML={{ __html: project.summary }}
                                />
                            </CardContent>
                            {project.url.href && (
                                <div className="p-6 pt-0 mt-auto">
                                    <Button variant="outline" asChild className="w-full">
                                        <a href={project.url.href} target="_blank" rel="noreferrer">
                                            View Project <ArrowRight className="ml-2 h-4 w-4" />
                                        </a>
                                    </Button>
                                </div>
                            )}
                        </Card>
                    ))}
                </div>
            </section>
        </div >
    )
}
