import { useParams, Link } from "react-router-dom"
import { useState, useEffect } from "react"
import { getBlogBySlug, type BlogPost } from "@/lib/api"
import { MarkdownBroadcaster } from "@/components/markdown/MarkdownBroadcaster"
import { Button } from "@/components/ui/button"
import { ArrowLeft, Calendar, Tag } from "lucide-react"
import { Badge } from "@/components/ui/badge"

export default function BlogDetailPage() {
    const { slug } = useParams<{ slug: string }>()
    const [post, setPost] = useState<BlogPost | null>(null)
    const [loading, setLoading] = useState(true)
    const [error, setError] = useState("")

    useEffect(() => {
        if (!slug) return

        const fetchPost = async () => {
            try {
                const data = await getBlogBySlug(slug)
                setPost(data)
            } catch (err) {
                setError(err instanceof Error ? err.message : "Failed to load post")
            } finally {
                setLoading(false)
            }
        }
        fetchPost()
    }, [slug])

    if (loading) {
        return (
            <div className="container py-16 text-center">
                <p className="text-muted-foreground">Loading...</p>
            </div>
        )
    }

    if (error || !post) {
        return (
            <div className="container py-16 text-center space-y-4">
                <h1 className="text-2xl font-bold">Post Not Found</h1>
                <p className="text-muted-foreground">{error || "The blog post you're looking for doesn't exist."}</p>
                <Button asChild>
                    <Link to="/blog">
                        <ArrowLeft className="mr-2 h-4 w-4" />
                        Back to Blog
                    </Link>
                </Button>
            </div>
        )
    }

    const date = new Date(post.publish_date * 1000).toLocaleDateString('en-US', {
        year: 'numeric',
        month: 'long',
        day: 'numeric'
    })

    const tags = post.tags ? post.tags.split(',').map(t => t.trim()) : []

    return (
        <div className="container py-10 max-w-3xl mx-auto">
            <Link to="/blog" className="text-muted-foreground hover:text-foreground inline-flex items-center mb-8">
                <ArrowLeft className="mr-2 h-4 w-4" />
                Back to Blog
            </Link>

            <article className="space-y-8">
                <header className="space-y-4">
                    <h1 className="text-4xl font-bold tracking-tight">{post.title}</h1>
                    {post.description && (
                        <p className="text-xl text-muted-foreground">{post.description}</p>
                    )}
                    <div className="flex flex-wrap items-center gap-4 text-sm text-muted-foreground">
                        <span className="flex items-center gap-1">
                            <Calendar className="h-4 w-4" />
                            {date}
                        </span>
                        {post.category && (
                            <Badge variant="secondary">{post.category}</Badge>
                        )}
                    </div>
                    {tags.length > 0 && (
                        <div className="flex items-center gap-2">
                            <Tag className="h-4 w-4 text-muted-foreground" />
                            {tags.map(tag => (
                                <Badge key={tag} variant="outline">{tag}</Badge>
                            ))}
                        </div>
                    )}
                </header>

                <hr />

                <div className="prose prose-lg dark:prose-invert max-w-none">
                    <MarkdownBroadcaster content={post.content} />
                </div>
            </article>
        </div>
    )
}
