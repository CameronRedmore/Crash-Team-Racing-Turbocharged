#include <common.h>
#include <platform/native_collision.h>
#include <math.h>

static NativePhysicsVec NC_Add(NativePhysicsVec a, NativePhysicsVec b) { return (NativePhysicsVec){a.x+b.x,a.y+b.y,a.z+b.z}; }
static NativePhysicsVec NC_Sub(NativePhysicsVec a, NativePhysicsVec b) { return (NativePhysicsVec){a.x-b.x,a.y-b.y,a.z-b.z}; }
static NativePhysicsVec NC_Scale(NativePhysicsVec a, double s) { return (NativePhysicsVec){a.x*s,a.y*s,a.z*s}; }
static double NC_Dot(NativePhysicsVec a, NativePhysicsVec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static NativePhysicsVec NC_Cross(NativePhysicsVec a, NativePhysicsVec b) { return (NativePhysicsVec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static NativePhysicsVec NC_Normalize(NativePhysicsVec a)
{
	double length = sqrt(NC_Dot(a,a));
	return length > 1e-12 ? NC_Scale(a,1/length) : (NativePhysicsVec){0,0,0};
}
static int NC_Barycentric(NativePhysicsVec p, NativePhysicsVec a, NativePhysicsVec b, NativePhysicsVec c, double *u, double *v)
{
	NativePhysicsVec e0=NC_Sub(b,a), e1=NC_Sub(c,a), delta=NC_Sub(p,a);
	double aa=NC_Dot(e0,e0), ab=NC_Dot(e0,e1), bb=NC_Dot(e1,e1);
	double denominator=aa*bb-ab*ab;
	if (fabs(denominator)<1e-12) return 0;
	*u=(bb*NC_Dot(delta,e0)-ab*NC_Dot(delta,e1))/denominator;
	*v=(aa*NC_Dot(delta,e1)-ab*NC_Dot(delta,e0))/denominator;
	return *u >= -1e-9 && *v >= -1e-9 && *u+*v <= 1+1e-9;
}
int NativeCollision_RayTriangle(NativePhysicsVec start, NativePhysicsVec end, NativePhysicsVec a,
                               NativePhysicsVec b, NativePhysicsVec c, NativeCollisionHit *hit, double *u, double *v)
{
	NativePhysicsVec normal=NC_Normalize(NC_Cross(NC_Sub(b,a),NC_Sub(c,a)));
	NativePhysicsVec direction=NC_Sub(end,start);
	double denominator=NC_Dot(direction,normal);
	if (fabs(denominator)<1e-12) return 0;
	double fraction=NC_Dot(NC_Sub(a,start),normal)/denominator;
	if (fraction < 0 || fraction > 1) return 0;
	NativePhysicsVec point=NC_Add(start,NC_Scale(direction,fraction));
	if (!NC_Barycentric(point,a,b,c,u,v)) return 0;
	*hit=(NativeCollisionHit){fraction,point,normal,0};
	return 1;
}
static void NC_Accept(NativeCollisionHit *hit, double t, NativePhysicsVec start, NativePhysicsVec delta,
                      NativePhysicsVec point, NativePhysicsVec fallback, int feature)
{
	if (t < -1e-9 || t > 1 || t >= hit->fraction) return;
	t=fmax(0,t);
	NativePhysicsVec normal=NC_Normalize(NC_Sub(NC_Add(start,NC_Scale(delta,t)),point));
	if (NC_Dot(normal,normal)<0.5) normal=fallback;
	// Contacts at t=0 only block inward motion; tangential movement can slide.
	if (NC_Dot(delta,normal)>=-1e-10) return;
	*hit=(NativeCollisionHit){t,point,normal,feature};
}
static int NC_Quadratic(double aa, double bb, double cc, double *t)
{
	if (cc <= 0) { *t=0; return 1; }
	if (aa<1e-18 || bb>=0) return 0;
	double discriminant=bb*bb-4*aa*cc;
	if (discriminant < 0) return 0;
	// Stable smaller root avoids cancellation when already near the surface.
	*t=2*cc/(-bb+sqrt(discriminant));
	return *t>=0 && *t<=1;
}
int NativeCollision_SweepTriangle(NativePhysicsVec start, NativePhysicsVec end, double radius,
                                 NativePhysicsVec a, NativePhysicsVec b, NativePhysicsVec c, NativeCollisionHit *hit)
{
	NativePhysicsVec delta=NC_Sub(end,start), normal=NC_Normalize(NC_Cross(NC_Sub(b,a),NC_Sub(c,a)));
	if (NC_Dot(normal,normal)<0.5 || radius<0) return 0;
	hit->fraction=2;
	double distance=NC_Dot(NC_Sub(start,a),normal), rate=NC_Dot(delta,normal);
	for (int side=-1; side<=1; side+=2)
	{
		if (fabs(rate)<1e-12) continue;
		double t=(side*radius-distance)/rate;
		if (fabs(distance)<=radius) t=0;
		NativePhysicsVec center=NC_Add(start,NC_Scale(delta,t));
		NativePhysicsVec point=NC_Sub(center,NC_Scale(normal,NC_Dot(NC_Sub(center,a),normal)));
		double u,v;
		if (NC_Barycentric(point,a,b,c,&u,&v)) NC_Accept(hit,t,start,delta,point,NC_Scale(normal,side),0);
	}
	NativePhysicsVec vertices[3]={a,b,c};
	for (int i=0;i<3;i++)
	{
		NativePhysicsVec vertex=vertices[i], edge=NC_Sub(vertices[(i+1)%3],vertex), offset=NC_Sub(start,vertex);
		double t, lengthSq=NC_Dot(edge,edge);
		if (lengthSq>1e-12)
		{
			NativePhysicsVec perpendicular=NC_Sub(offset,NC_Scale(edge,NC_Dot(offset,edge)/lengthSq));
			NativePhysicsVec velocity=NC_Sub(delta,NC_Scale(edge,NC_Dot(delta,edge)/lengthSq));
			if (NC_Quadratic(NC_Dot(velocity,velocity),2*NC_Dot(perpendicular,velocity),NC_Dot(perpendicular,perpendicular)-radius*radius,&t))
			{
				double along=NC_Dot(NC_Add(offset,NC_Scale(delta,t)),edge)/lengthSq;
				if (along>=0 && along<=1) NC_Accept(hit,t,start,delta,NC_Add(vertex,NC_Scale(edge,along)),normal,1+i);
			}
		}
		if (NC_Quadratic(NC_Dot(delta,delta),2*NC_Dot(offset,delta),NC_Dot(offset,offset)-radius*radius,&t))
			NC_Accept(hit,t,start,delta,vertex,normal,4+i);
	}
	return hit->fraction<=1;
}

// This context exists only during a synchronous BSP sweep; no runtime pointers
// are persisted in checkpoints. Queries outside a player sweep use their input.
static struct
{
	struct ScratchpadStruct *sps;
	NativePhysicsVec start, end, normal;
	SVec3 exportedNormal;
	double fraction;
} s_collisionSweep;
void NativeCollision_EndSweep(void) { s_collisionSweep.sps=NULL; }
NativePhysicsVec NativeCollision_Normal(struct ScratchpadStruct *sps)
{
	SVec3 n=sps->hit.plane.normal;
	if (s_collisionSweep.sps==sps && n.x==s_collisionSweep.exportedNormal.x && n.y==s_collisionSweep.exportedNormal.y && n.z==s_collisionSweep.exportedNormal.z)
		return s_collisionSweep.normal;
	return (NativePhysicsVec){n.x,n.y,n.z};
}
void NativeCollision_BeginSweep(struct ScratchpadStruct *sps, NativePhysicsVec start, NativePhysicsVec step)
{
	s_collisionSweep.sps=sps;
	s_collisionSweep.start=start;
	s_collisionSweep.end=NC_Add(start,step);
	s_collisionSweep.fraction=1;
	s_collisionSweep.exportedNormal=(SVec3){0};
	s_collisionSweep.normal=(NativePhysicsVec){0,0,0};
}
double NativeCollision_HitFraction(struct ScratchpadStruct *sps)
{
	if (s_collisionSweep.sps==sps && (s32)round(s_collisionSweep.fraction*4096)==sps->hitFraction) return s_collisionSweep.fraction;
	return sps->hitFraction/4096.0;
}
static NativePhysicsVec NC_ClosestOnTriangle(NativePhysicsVec p, NativePhysicsVec a, NativePhysicsVec b, NativePhysicsVec c)
{
	NativePhysicsVec ab=NC_Sub(b,a), ac=NC_Sub(c,a), ap=NC_Sub(p,a);
	double d1=NC_Dot(ab,ap), d2=NC_Dot(ac,ap);
	if (d1<=0 && d2<=0) return a;
	NativePhysicsVec bp=NC_Sub(p,b);
	double d3=NC_Dot(ab,bp), d4=NC_Dot(ac,bp);
	if (d3>=0 && d4<=d3) return b;
	double vc=d1*d4-d3*d2;
	if (vc<=0 && d1>=0 && d3<=0) return NC_Add(a,NC_Scale(ab,d1/(d1-d3)));
	NativePhysicsVec cp=NC_Sub(p,c);
	double d5=NC_Dot(ab,cp), d6=NC_Dot(ac,cp);
	if (d6>=0 && d5<=d6) return c;
	double vb=d5*d2-d1*d6;
	if (vb<=0 && d2>=0 && d6<=0) return NC_Add(a,NC_Scale(ac,d2/(d2-d6)));
	double va=d3*d6-d5*d4;
	if (va<=0 && (d4-d3)>=0 && (d5-d6)>=0) return NC_Add(b,NC_Scale(NC_Sub(c,b),(d4-d3)/((d4-d3)+(d5-d6))));
	double denominator=va+vb+vc;
	if (fabs(denominator)<1e-12) return a;
	return NC_Add(a,NC_Add(NC_Scale(ab,vb/denominator),NC_Scale(ac,vc/denominator)));
}
static NativePhysicsVec NC_Vertex(const SVec3 *v) { return (NativePhysicsVec){v->x,v->y,v->z}; }
static SVec3 NC_Export(NativePhysicsVec v) { return (SVec3){.x=(s16)round(v.x),.y=(s16)round(v.y),.z=(s16)round(v.z)}; }
static int NC_OverlapsTriangle(NativePhysicsVec point, double radius,
                               NativePhysicsVec a, NativePhysicsVec b, NativePhysicsVec c)
{
	NativePhysicsVec normal=NC_Normalize(NC_Cross(NC_Sub(b,a),NC_Sub(c,a)));
	if (NC_Dot(normal,normal)<0.5) return 0;
	double distance=NC_Dot(NC_Sub(point,a),normal), u,v;
	if (fabs(distance)>radius+1e-9) return 0;
	NativePhysicsVec projected=NC_Sub(point,NC_Scale(normal,distance));
	if (NC_Barycentric(projected,a,b,c,&u,&v)) return 1;
	NativePhysicsVec vertices[3]={a,b,c};
	for (int i=0;i<3;i++)
	{
		NativePhysicsVec edge=NC_Sub(vertices[(i+1)%3],vertices[i]);
		double lengthSq=NC_Dot(edge,edge);
		double t=lengthSq>1e-12 ? fmax(0,fmin(1,NC_Dot(NC_Sub(point,vertices[i]),edge)/lengthSq)) : 0;
		NativePhysicsVec offset=NC_Sub(point,NC_Add(vertices[i],NC_Scale(edge,t)));
		if (NC_Dot(offset,offset)<=radius*radius+1e-9) return 1;
	}
	return 0;
}
void NativeCollision_MovedTriangle(struct ScratchpadStruct *sps, struct BspSearchVertex *a, struct BspSearchVertex *b, struct BspSearchVertex *c)
{
	struct QuadBlock *quad=sps->candidate.ptrQuadblock;
	u16 flags=quad->quadFlags;
	if ((flags & QUADBLOCK_FLAG_DOOR) && ((s8)quad->terrain_type & sdata->doorAccessFlags)) return;
	NativePhysicsVec start=NC_Vertex(&sps->Union.QuadBlockColl.pos), end=NC_Vertex(&sps->Input1.pos);
	if (s_collisionSweep.sps==sps) { start=s_collisionSweep.start; end=s_collisionSweep.end; }
	NativePhysicsVec authoredNormal=NC_Vertex(&a->plane.normal);
	if (!(flags & QUADBLOCK_FLAG_TRIGGER) && !(quad->draw_order_low & QUADBLOCK_DRAW_ORDER_LOW_DOUBLE_SIDED) &&
	    NC_Dot(NC_Sub(start,NC_Vertex(&a->pos)),authoredNormal)<0) return;
	NativeCollisionHit hit;
	sps->numTrianglesTested++;
	if (flags & QUADBLOCK_FLAG_TRIGGER)
	{
		// Triggers detect overlap even during stationary or tangential contact.
		// The blocking sweep intentionally rejects those contacts for solids.
		if (NC_OverlapsTriangle(start,sps->Input1.hitRadius,NC_Vertex(&a->pos),NC_Vertex(&b->pos),NC_Vertex(&c->pos)) ||
		    NC_OverlapsTriangle(end,sps->Input1.hitRadius,NC_Vertex(&a->pos),NC_Vertex(&b->pos),NC_Vertex(&c->pos)) ||
		    NativeCollision_SweepTriangle(start,end,sps->Input1.hitRadius,NC_Vertex(&a->pos),NC_Vertex(&b->pos),NC_Vertex(&c->pos),&hit))
			sps->collision.stepFlags|=(u8)quad->terrain_type;
		return;
	}
	if (!NativeCollision_SweepTriangle(start,end,sps->Input1.hitRadius,NC_Vertex(&a->pos),NC_Vertex(&b->pos),NC_Vertex(&c->pos),&hit)) return;
	if (flags & QUADBLOCK_FLAG_GROUND)
	{
		// Road triangles form a continuous surface. Their sphere/edge normals
		// are not road slopes: projecting forward motion onto one launches the
		// kart at tessellation seams. Respond only to motion into the face.
		NativePhysicsVec normal=NC_Normalize(NC_Cross(NC_Sub(NC_Vertex(&b->pos),NC_Vertex(&a->pos)),
		                                           NC_Sub(NC_Vertex(&c->pos),NC_Vertex(&a->pos))));
		if (NC_Dot(normal,hit.normal)<0) normal=NC_Scale(normal,-1);
		if (NC_Dot(NC_Sub(end,start),normal)>=-1e-10) return;
		hit.normal=normal;
	}
	if (flags & QUADBLOCK_FLAG_NO_COLLISION_RESPONSE)
	{
		if (flags & QUADBLOCK_FLAG_KILL_PLANE) sps->collision.stepFlags|=COLL_STEP_FLAG_KILL_PLANE;
		return;
	}
	if (hit.fraction>=NativeCollision_HitFraction(sps)) return;
	sps->hitFraction=(s32)round(hit.fraction*4096);
	if (s_collisionSweep.sps==sps) s_collisionSweep.fraction=hit.fraction;
	sps->hitLevelTriangle.v0=a->pLevelVertex; sps->hitLevelTriangle.v1=b->pLevelVertex; sps->hitLevelTriangle.v2=c->pLevelVertex;
	sps->hitBspSearchTriangle.v0=a; sps->hitBspSearchTriangle.v1=b; sps->hitBspSearchTriangle.v2=c;
	// Like retail, the exported hit position is the triangle point nearest the
	// requested end of the sweep, not the contact point. Surface pushback and
	// rollback normals compare it with the driver's position after movement;
	// a contact at the start of a long step otherwise reads as penetration
	// along the direction of travel and adds speed every frame on slopes.
	NativePhysicsVec av=NC_Vertex(&a->pos), bv=NC_Vertex(&b->pos), cv=NC_Vertex(&c->pos);
	NativePhysicsVec faceNormal=NC_Normalize(NC_Cross(NC_Sub(bv,av),NC_Sub(cv,av)));
	sps->hit.hitPos=NC_Export(NC_ClosestOnTriangle(end,av,bv,cv));
	sps->hit.pushOut=NC_Export(NC_Sub(end,NC_Scale(faceNormal,NC_Dot(NC_Sub(end,av),faceNormal))));
	sps->hit.normalAxis=a->normalAxis;
	sps->hit.plane=a->plane;
	sps->hit.plane.normal=NC_Export(NC_Scale(hit.normal,4096));
	if (s_collisionSweep.sps==sps)
	{
		s_collisionSweep.normal=NC_Scale(hit.normal,4096);
		s_collisionSweep.exportedNormal=sps->hit.plane.normal;
	}
	sps->hit.ptrQuadblock=quad;
	sps->hit.triangleID=sps->candidate.triangleID;
	sps->hit.reorderResult=hit.feature==0 ? COLL_TRIANGLE_CLIP_FACE : COLL_TRIANGLE_CLIP_EDGE_V1_V2;
	sps->Union.QuadBlockColl.hitPos=NC_Export(NC_Add(start,NC_Scale(NC_Sub(end,start),hit.fraction)));
	sps->boolDidTouchQuadblock++;
}
void NativeCollision_FixedTriangle(struct ScratchpadStruct *sps, struct BspSearchVertex *a, struct BspSearchVertex *b, struct BspSearchVertex *c)
{
	NativePhysicsVec start=NC_Vertex(&sps->Union.QuadBlockColl.pos), end=NC_Vertex(&sps->Union.QuadBlockColl.hitPos);
	if (NC_Dot(NC_Sub(end,start),NC_Vertex(&sps->candidate.plane.normal))>=0) return;
	NativeCollisionHit hit;
	double u,v;
	if (!NativeCollision_RayTriangle(start,end,NC_Vertex(&a->pos),NC_Vertex(&b->pos),NC_Vertex(&c->pos),&hit,&u,&v)) return;
	struct QuadBlock *quad=sps->candidate.ptrQuadblock;
	if (quad->quadFlags & QUADBLOCK_FLAG_TRIGGER) { sps->collision.stepFlags|=(u8)quad->terrain_type; return; }
	sps->hit.ptrQuadblock=quad;
	sps->hitBarycentrics.v1=(s16)round(u*4096); sps->hitBarycentrics.v2=(s16)round(v*4096);
	sps->hitLevelTriangle.v0=a->pLevelVertex; sps->hitLevelTriangle.v1=b->pLevelVertex; sps->hitLevelTriangle.v2=c->pLevelVertex;
	sps->hit.hitPos=NC_Export(hit.point);
	sps->Union.QuadBlockColl.hitPos=sps->hit.hitPos;
	sps->hit.plane=sps->candidate.plane;
	sps->boolDidTouchQuadblock++;
}
